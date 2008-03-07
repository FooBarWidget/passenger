require 'support/config'
require 'abstract_server_spec'

shared_examples_for "a spawn server" do
	it_should_behave_like "AbstractServer"
	
	it "should raise an AbstractServer::ServerError if the server was killed" do
		Process.kill('SIGABRT', @spawner.server_pid)
		spawning = lambda { spawn_application }
		spawning.should raise_error(AbstractServer::ServerError)
	end
	
	it "should work correctly after a restart, if something went wrong" do
		Process.kill('SIGABRT', @spawner.server_pid)
		spawning = lambda { spawn_application }
		spawning.should raise_error(AbstractServer::ServerError)
		
		@spawner.stop
		@spawner.start
		app = spawn_application
		app.pid.should_not == 0
		app.app_root.should_not be_nil
		app.close
	end
end
