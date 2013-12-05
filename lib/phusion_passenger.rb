# encoding: utf-8
#  Phusion Passenger - https://www.phusionpassenger.com/
#  Copyright (c) 2010-2013 Phusion
#
#  "Phusion Passenger" is a trademark of Hongli Lai & Ninh Bui.
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.

module PhusionPassenger
	FILE_LOCATION = File.expand_path(__FILE__)
	
	
	###### Names and version numbers ######
	
	PACKAGE_NAME = 'passenger'
	# Run 'rake ext/common/Constants.h' after changing this number.
	VERSION_STRING = '4.0.26-sails'
	
	PREFERRED_NGINX_VERSION = '1.4.4'
	NGINX_SHA256_CHECKSUM = '7c989a58e5408c9593da0bebcd0e4ffc3d892d1316ba5042ddb0be5b0b4102b9'

	PREFERRED_PCRE_VERSION  = '8.32'
	PCRE_SHA256_CHECKSUM = 'd5d8634b36baf3d08be442a627001099583b397f456bc795304a013383b6423a'

	STANDALONE_INTERFACE_VERSION  = 1
	
	
	###### Directories ######
	
	GLOBAL_NAMESPACE_DIRNAME            = "passenger"
	# Subdirectory under $HOME to use for storing stuff.
	USER_NAMESPACE_DIRNAME              = ".passenger"
	
	# Directories in which to look for plugins.
	PLUGIN_DIRS = [
		"/usr/share/#{GLOBAL_NAMESPACE_DIRNAME}/plugins",
		"/usr/local/share/#{GLOBAL_NAMESPACE_DIRNAME}/plugins",
		"~/#{USER_NAMESPACE_DIRNAME}/plugins"
	]
	
	REQUIRED_LOCATIONS_INI_FIELDS = [
		:bin_dir,
		:agents_dir,
		:lib_dir,
		:helper_scripts_dir,
		:resources_dir,
		:include_dir,
		:doc_dir,
		:ruby_libdir,
		:node_libdir,
		:apache2_module_path,
		:ruby_extension_source_dir,
		:nginx_module_source_dir
	].freeze
	OPTIONAL_LOCATIONS_INI_FIELDS = [
		# Directory in which downloaded Phusion Passenger binaries are stored.
		# Only available when originally packaged.
		:download_cache_dir
	].freeze
	
	# Follows the logic of ext/common/ResourceLocator.h, so don't forget to modify that too.
	def self.locate_directories(source_root_or_location_configuration_file = nil)
		source_root_or_location_configuration_file ||= find_location_configuration_file
		root_or_file = @source_root = source_root_or_location_configuration_file
		
		if root_or_file && File.file?(root_or_file)
			filename = root_or_file
			options  = parse_ini_file(filename)
			
			@natively_packaged     = get_bool_option(filename, options, 'natively_packaged')
			REQUIRED_LOCATIONS_INI_FIELDS.each do |field|
				instance_variable_set("@#{field}", get_option(filename, options, field.to_s).freeze)
			end
			OPTIONAL_LOCATIONS_INI_FIELDS.each do |field|
				instance_variable_set("@#{field}", get_option(filename, options, field.to_s, false).freeze)
			end
		else
			@source_root           = File.dirname(File.dirname(FILE_LOCATION))
			@natively_packaged     = false
			@bin_dir               = "#{@source_root}/bin".freeze
			@agents_dir            = "#{@source_root}/buildout/agents".freeze
			@lib_dir               = "#{@source_root}/buildout".freeze
			@helper_scripts_dir    = "#{@source_root}/helper-scripts".freeze
			@resources_dir         = "#{@source_root}/resources".freeze
			@include_dir           = "#{@source_root}/ext".freeze
			@doc_dir               = "#{@source_root}/doc".freeze
			@ruby_libdir           = File.dirname(FILE_LOCATION)
			@node_libdir           = "#{@source_root}/node_lib".freeze
			@apache2_module_path   = "#{@source_root}/buildout/apache2/mod_passenger.so".freeze
			@ruby_extension_source_dir = "#{@source_root}/ext/ruby"
			@nginx_module_source_dir   = "#{@source_root}/ext/nginx"
			@download_cache_dir    = "#{@source_root}/download_cache"
			REQUIRED_LOCATIONS_INI_FIELDS.each do |field|
				if instance_variable_get("@#{field}").nil?
					raise "BUG: @#{field} not set"
				end
			end
		end
	end
	
	# Returns whether this Phusion Passenger installation is in the 'originally packaged'
	# configuration (as opposed to the 'natively packaged' configuration.
	def self.originally_packaged?
		return !@natively_packaged
	end

	def self.natively_packaged?
		return @natively_packaged
	end

	# Whether the current Phusion Passenger installation is installed
	# from a release package, e.g. an official gem or official tarball.
	# Retruns false if e.g. the gem was built by the user, or if this
	# install is from a git repository.
	def self.installed_from_release_package?
		File.exist?("#{resources_dir}/release.txt")
	end

	# When originally packaged, returns the source root.
	# When natively packaged, returns the location of the location configuration file.
	def self.source_root
		return @source_root
	end

	# Generate getters for the directory types in locations.ini.
	getters_code = ""
	@ruby_libdir = File.dirname(FILE_LOCATION)
	(REQUIRED_LOCATIONS_INI_FIELDS + OPTIONAL_LOCATIONS_INI_FIELDS).each do |field|
		getters_code << %Q{
			def self.#{field}
				return @#{field}
			end
		}
	end
	eval(getters_code, binding, __FILE__, __LINE__)

	def self.index_doc_path
		return "#{doc_dir}/#{INDEX_DOC_NAME}"
	end

	def self.apache2_doc_path
		return "#{doc_dir}/#{APACHE2_DOC_NAME}"
	end

	def self.nginx_doc_path
		return "#{doc_dir}/#{NGINX_DOC_NAME}"
	end

	def self.standalone_doc_path
		return "#{doc_dir}/#{STANDALONE_DOC_NAME}"
	end
	
	
	###### Other resource locations ######
	
	BINARIES_URL_ROOT   = "https://oss-binaries.phusionpassenger.com/binaries/passenger/by_release"
	INDEX_DOC_NAME      = "Users guide.html"
	APACHE2_DOC_NAME    = "Users guide Apache.html"
	NGINX_DOC_NAME      = "Users guide Nginx.html"
	STANDALONE_DOC_NAME = "Users guide Standalone.html"

	def self.binaries_ca_cert_path
		return "#{resources_dir}/oss-binaries.phusionpassenger.com.crt"
	end
	
	
	if !$LOAD_PATH.include?(ruby_libdir)
		$LOAD_PATH.unshift(ruby_libdir)
		$LOAD_PATH.uniq!
	end


private
	def self.find_location_configuration_file
		filename = ENV['PASSENGER_LOCATION_CONFIGURATION_FILE']
		return filename if filename && !filename.empty?

		filename = File.dirname(FILE_LOCATION) + "/phusion_passenger/locations.ini"
		return filename if filename && File.exist?(filename)

		require 'etc' if !defined?(Etc)
		begin
			home_dir = Etc.getpwuid(Process.uid).dir
		rescue ArgumentError
			# Unknown user.
			home_dir = ENV['HOME']
		end
		if home_dir && !home_dir.empty?
			filename = "#{home_dir}/#{USER_NAMESPACE_DIRNAME}/locations.ini"
			return filename if File.exist?(filename)
		end

		filename = "/etc/#{GLOBAL_NAMESPACE_DIRNAME}/locations.ini"
		return filename if File.exist?(filename)

		return nil
	end

	def self.parse_ini_file(filename)
		options  = {}
		in_locations_section = false
		File.open(filename, 'r') do |f|
			while !f.eof?
				line = f.readline
				line.strip!
				next if line.empty?
				if line =~ /\A\[(.+)\]\Z/
					in_locations_section = $1 == 'locations'
				elsif in_locations_section && line =~ /=/
					key, value = line.split(/ *= */, 2)
					options[key.freeze] = value.freeze
				end
			end
		end
		return options
	end

	def self.get_option(filename, options, key, required = true)
		value = options[key]
		if value
			return value
		elsif required
			raise "Option '#{key}' missing in file '#{filename}'"
		else
			return nil
		end
	end
	
	def self.get_bool_option(filename, options, key)
		value = get_option(filename, options, key)
		return value == 'yes' || value == 'true' || value == 'on' || value == '1'
	end
end if !defined?(PhusionPassenger::VERSION_STRING)
