require 'rubygems'
require 'socket'
require 'etc'
require 'passenger/abstract_server'
require 'passenger/application'
require 'passenger/utils'
require 'passenger/request_handler'

begin
	# Preload MySQL if possible. We want to preload it and we need
	# its exception classes.
	require 'mysql'
rescue LoadError
end
begin
	# Preload SQLite 3 if possible. Rails 2.0 apps use it by default.
	require 'sqlite3'
rescue LoadError
end

module Passenger

# An abstract base class for AppInitError and FrameworkInitError. This represents
# the failure when initializing something.
class InitializationError < StandardError
	# The exception that caused initialization to fail. This may be nil.
	attr_accessor :child_exception

	# Create a new InitializationError. +message+ is the error message,
	# and +child_exception+ is the exception that caused initialization
	# to fail.
	def initialize(message, child_exception = nil)
		super(message)
		@child_exception = child_exception
	end
end

# Raised when ApplicationSpawner, FrameworkSpawner or SpawnManager was unable
# spawn a Ruby on Rails application, because the application either threw an
# exception or called exit.
#
# If the +child_exception+ attribute is nil, then it means that the application
# called exit.
class AppInitError < InitializationError
end

# This class is capable of spawns instances of a single Ruby on Rails application.
# It does so by preloading as much of the application's code as possible, then creating
# instances of the application using what is already preloaded. This makes it spawning
# application instances very fast, except for the first spawn.
#
# Use multiple instances of ApplicationSpawner if you need to spawn multiple different
# Ruby on Rails applications.
#
# *Note*: ApplicationSpawner may only be started asynchronously with AbstractServer#start.
# Starting it synchronously with AbstractServer#start_synchronously has not been tested.
class ApplicationSpawner < AbstractServer
	include Utils
	
	# This exception means that the ApplicationSpawner server process exited unexpectedly.
	class Error < AbstractServer::ServerError
	end
	
	# The user ID of the root user.
	ROOT_UID = 0
	# The group ID of the root user.
	ROOT_GID = 0
	
	# An attribute, used internally. This should not be used outside Passenger.
	attr_accessor :time

	# +app_root+ is the root directory of this application, i.e. the directory
	# that contains 'app/', 'public/', etc. If given an invalid directory,
	# or a directory that doesn't appear to be a Rails application root directory,
	# then an ArgumentError will be raised.
	#
	# If +lower_privilege+ is true, then ApplicationSpawner will attempt to
	# switch to the user who owns the application's <tt>config/environment.rb</tt>,
	# and to the default group of that user.
	#
	# If that user doesn't exist on the system, or if that user is root,
	# then ApplicationSpawner will attempt to switch to the username given by
	# +lowest_user+ (and to the default group of that user).
	# If +lowest_user+ doesn't exist either, or if switching user failed
	# (because the current process does not have the privilege to do so),
	# then ApplicationSpawner will continue without reporting an error.
	def initialize(app_root, lower_privilege = true, lowest_user = "nobody")
		super()
		begin
			@app_root = normalize_path(app_root)
		rescue SystemCallError => e
			raise ArgumentError, e.message
		rescue ArgumentError
			raise
		end
		@lower_privilege = lower_privilege
		@lowest_user = lowest_user
		self.time = Time.now
		assert_valid_app_root(@app_root)
		define_message_handler(:spawn_application, :handle_spawn_application)
	end
	
	# Spawn an instance of the RoR application. When successful, an Application object
	# will be returned, which represents the spawned RoR application.
	#
	# Raises:
	# - AbstractServer::ServerNotStarted: The ApplicationSpawner server hasn't already been started.
	# - ApplicationSpawner::Error: The ApplicationSpawner server exited unexpectedly.
	def spawn_application
		server.write("spawn_application")
		pid, socket_name, using_abstract_namespace = server.read
		if pid.nil?
			raise IOError, "Connection closed"
		end
		owner_pipe = server.recv_io
		return Application.new(@app_root, pid, socket_name,
			using_abstract_namespace == "true", owner_pipe)
	rescue SystemCallError, IOError, SocketError => e
		raise Error, "The application spawner server exited unexpectedly"
	end
	
	# Overrided from AbstractServer#start.
	#
	# May raise these additional exceptions:
	# - AppInitError: The Ruby on Rails application raised an exception
	#   or called exit() during startup.
	# - ApplicationSpawner::Error: The ApplicationSpawner server exited unexpectedly.
	def start
		super
		begin
			status = server.read[0]
			if status == 'exception'
				child_exception = unmarshal_exception(server.read_scalar)
				stop
				raise AppInitError.new(
					"Application '#{@app_root}' raised an exception: " <<
					"#{child_exception.class} (#{child_exception.message})",
					child_exception)
			elsif status == 'exit'
				stop
				raise AppInitError.new("Application '#{@app_root}' exited during startup")
			end
		rescue IOError, SystemCallError, SocketError
			stop
			raise Error, "The application spawner server exited unexpectedly"
		end
	end

protected
	# Overrided method.
	def before_fork # :nodoc:
		if GC.copy_on_write_friendly?
			# Garbage collect now so that the child process doesn't have to
			# do that (to prevent making pages dirty).
			GC.start
		end
	end
	
	# Overrided method.
	def initialize_server # :nodoc:
		begin
			$0 = "Passenger ApplicationSpawner: #{@app_root}"
			Dir.chdir(@app_root)
			lower_privilege! if @lower_privilege
			preload_application
		rescue StandardError, ScriptError, NoMemoryError => e
			client.write('exception')
			client.write_scalar(marshal_exception(e))
			return
		rescue SystemExit
			client.write('exit')
			raise
		end
		client.write('success')
	end
	
private
	def lower_privilege!
		stat = File.stat("config/environment.rb")
		begin
			if !switch_to_user(stat.uid)
				switch_to_user(@lowest_user)
			end
		rescue Errno::EPERM
			# No problem if we were unable to switch user.
		end
	end

	def switch_to_user(user)
		begin
			if user.is_a?(String)
				pw = Etc.getpwnam(user)
				uid = pw.uid
				gid = pw.gid
			else
				uid = user
				gid = Etc.getpwuid(uid).gid
			end
		rescue
			return false
		end
		if uid == ROOT_UID
			return false
		else
			Process::Sys.setgid(gid)
			Process::Sys.setuid(uid)
			return true
		end
	end

	def preload_application
		Object.const_set(:RAILS_ROOT, @app_root)
		if defined?(Rails::Initializer)
			Rails::Initializer.run(:set_load_path)
		end
		require 'config/environment'
		require_dependency 'application'
		Dir.glob('app/{models,controllers,helpers}/*.rb').each do |file|
			require_dependency normalize_path(file)
		end
	end

	def handle_spawn_application
		# Double fork to prevent zombie processes.
		pid = fork do
			begin
				pid = fork do
					begin
						start_request_handler
					rescue SignalException => signal
						if e.message != RequestHandler::HARD_TERMINATION_SIGNAL &&
						   e.message != RequestHandler::SOFT_TERMINATION_SIGNAL
							print_exception('application', e)
						end
					rescue Exception => e
						print_exception('application', e)
					ensure
						exit!
					end
				end
			rescue Exception => e
				print_exception(self.class.to_s, e)
			ensure
				exit!
			end
		end
		Process.waitpid(pid)
	end
	
	def start_request_handler
		$0 = "Rails: #{@app_root}"
		reader, writer = IO.pipe
		begin
			handler = RequestHandler.new(reader)
			client.write(Process.pid, handler.socket_name,
				handler.using_abstract_namespace?)
			client.send_io(writer)
			writer.close
			client.close
			handler.main_loop
		ensure
			client.close rescue nil
			writer.close rescue nil
			handler.cleanup rescue nil
		end
	end
end

end # module Passenger
