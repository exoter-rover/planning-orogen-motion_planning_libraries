require 'vizkit'

Orocos.initialize

# Add as many (remote) hosts as necessary.
hosts = []
hosts << "exoter"
# ...

# Add every host to the name service.
# We use an asynchronous connection for the GUI.
hosts.each do |host|    
    Orocos::Async.name_service << Orocos::Async::CORBA::NameService.new(host)
end

# Initialize and configure CompoundDisplay widget.
compound_display = Vizkit.default_loader.CompoundDisplay
compound_display.set_grid_dimensions(1,1) # 2 rows, 4 columns
#compound_display.show_menu(false)
compound_display.show


# Configure CompoundDisplay manually in the script. 
compound_display.configure(0, "vicon", "pose_samples", 			"RigidBodyStateVisualization")
compound_display.configure(0, "vicon", "pose_samples", 			"TrajectoryVisualization")
compound_display.configure(0, "path_planner", "start_pose_samples", "RigidBodyStateVisualization")
compound_display.configure(0, "path_planner", "goal_pose_samples", 	"RigidBodyStateVisualization")
compound_display.configure(0, "path_planner", "trajectory", 		"TrajectoryVisualization")



# Initialize TaskInspector. You can drag output ports 
# from there to the CompoundDisplay.
inspector = Vizkit.default_loader.TaskInspector
inspector.show

# Add the previously defined name service. Do this only once, since we
# already appended all remote hosts to the name service before.
inspector.add_name_service(Orocos::Async.name_service)

Vizkit.exec