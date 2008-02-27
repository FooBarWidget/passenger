shared_examples_for "a minimal spawner" do
	it "should be able to spawn our stub application" do
		app = spawn_application
		app.pid.should_not == 0
		app.app_root.should_not be_nil
		app.close
	end
	
	it "should be able to spawn an arbitary number of applications" do
		last_pid = 0
		4.times do
			app = spawn_application
			app.pid.should_not == last_pid
			app.app_root.should_not be_nil
			last_pid = app.pid
			app.close
		end
	end
end
