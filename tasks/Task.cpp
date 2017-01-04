/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "Task.hpp"

#include <base/logging/logging_printf_style.h>

#include <envire/Orocos.hpp>

using namespace motion_planning_libraries;

Task::Task(std::string const& name)
    : TaskBase(name)
{
}

Task::Task(std::string const& name, RTT::ExecutionEngine* engine)
    : TaskBase(name, engine)
{
}

Task::~Task()
{
}

/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See Task.hpp for more detailed
// documentation about them.

bool Task::configureHook()
{
    if (! TaskBase::configureHook())
        return false;
    
    mpMotionPlanningLibraries = boost::shared_ptr<MotionPlanningLibraries>(
            new MotionPlanningLibraries(_config.get()));
    
    return true;
}
bool Task::startHook()
{
    if (! TaskBase::startHook())
        return false;
    return true;
}
void Task::updateHook()
{
    TaskBase::updateHook();
    base::Time t0 = base::Time::now();
    // Set traversability map.
    envire::OrocosEmitter::Ptr binary_event;
    // Use a loop here because binary_events could contain partial updates
    bool new_map = false;
    while(_traversability_map.read(binary_event) == RTT::NewData)
    {
        mEnv.applyEvents(*binary_event);   
        new_map = true;
    }
    if(new_map) {
        mpMotionPlanningLibraries->setTravGrid(&mEnv, _traversability_map_id);
        // Just wait some time for a new goal pose in case both
        // map and new goal have been sent together.
    }
     
    // Set start state / pose. 
    if(_start_state.connected()) {
        if(_start_state.readNewest(mStartState) == RTT::NewData) {
            if(mpMotionPlanningLibraries->setStartState(mStartState)) {
                mStartPose = mStartState.mPose;
                _start_pose_samples_debug.write(mStartPose);
            }
        }
    } else {   
        if(_start_pose_samples.readNewest(mStartPose) == RTT::NewData) {
            if(mpMotionPlanningLibraries->setStartState(State(mStartPose))) {
                _start_pose_samples_debug.write(mStartPose);
            }
        }
    }
    
    // Set goal state / pose.
    if(_goal_state.connected()) {
         if(_goal_state.readNewest(mGoalState) == RTT::NewData) {
            if(mpMotionPlanningLibraries->setGoalState(mGoalState)) {
                mGoalPose = mGoalState.mPose;
                _goal_pose_samples_debug.write(mGoalPose);
            }
        }
    } else {
        if(_goal_pose_samples.readNewest(mGoalPose) == RTT::NewData) {
            if(mpMotionPlanningLibraries->setGoalState(State(mGoalPose))) {
                _goal_pose_samples_debug.write(mGoalPose);
            }
        }
    }

    double cost = 0.0;
    std::vector <base::Waypoint > path;
    path.clear();

    if(!mpMotionPlanningLibraries->plan(_planning_time_sec, cost)) {
        enum MplErrors err = mpMotionPlanningLibraries->getError();
        if(err != MPL_ERR_NONE && err != MPL_ERR_REPLANNING_NOT_REQUIRED) {
            LOG_WARN("Planning could not be finished");
        }
        switch(err) {
            case MPL_ERR_NONE: break; // Does not change the current state.
            case MPL_ERR_REPLANNING_NOT_REQUIRED: break; // Does not change the current state.
            case MPL_ERR_MISSING_START: state(MISSING_START); break;
            case MPL_ERR_MISSING_GOAL: state(MISSING_GOAL); break;
            case MPL_ERR_MISSING_TRAV: state(MISSING_TRAV); break;
            case MPL_ERR_MISSING_START_GOAL: state(MISSING_START_GOAL); break;
            case MPL_ERR_MISSING_START_TRAV: state(MISSING_START_TRAV); break;
            case MPL_ERR_MISSING_GOAL_TRAV: state(MISSING_GOAL_TRAV); break;
            case MPL_ERR_MISSING_START_GOAL_TRAV: state(MISSING_START_GOAL_TRAV); break;
            case MPL_ERR_PLANNING_FAILED: state(PLANNING_FAILED); break;
            case MPL_ERR_WRONG_STATE_TYPE: state(WRONG_STATE_TYPE); break;
            case MPL_ERR_INITIALIZE_MAP: state(INITIALIZE_MAP_ERROR); break;
            case MPL_ERR_START_ON_OBSTACLE: state(START_ON_OBSTACLE); break;
            case MPL_ERR_GOAL_ON_OBSTACLE: state(GOAL_ON_OBSTACLE); break;
            case MPL_ERR_START_GOAL_ON_OBSTACLE: state(START_GOAL_ON_OBSTACLE); break;
            case MPL_ERR_SET_START_GOAL: state(SET_START_GOAL_ERROR); break;
            default: state(UNDEFINED_ERROR); break;
        }
        
        // In case of a real error an ewmpty trajectory will be sent.
        if(err != MPL_ERR_NONE && err != MPL_ERR_REPLANNING_NOT_REQUIRED) {
            std::vector<base::Trajectory> empty_trajectories;
            _trajectory.write(empty_trajectories);
            _waypoints.write(path);
        }
        
        if(err == MPL_ERR_START_ON_OBSTACLE || err == MPL_ERR_START_GOAL_ON_OBSTACLE) {
            LOG_INFO("Start state lies on an obstacle, tries to generate an escape trajectory");
            if(generateEscapeTrajectory()) {
                LOG_INFO("Escape trajectory could be created");
            } else {
                LOG_WARN("Escape trajectory could not be created");
            }
        }
    } else {
        state(RUNNING);
       
        // Compare new and old path and just publish and print path if its new.
        path = mpMotionPlanningLibraries->getPathInWorld();
        
        bool new_path = false;
        if(path.size() != mLastPath.size()) {
            new_path = true;
        } else {
            std::vector <base::Waypoint >::iterator it_new = path.begin();
            std::vector <base::Waypoint >::iterator it_last = mLastPath.begin();
            for(; it_new < path.end() && it_last < mLastPath.end(); it_new++, it_last++) {
                if(it_new->position != it_last->position || 
                    it_new->heading != it_last->heading) {
                    new_path = true;
                    break;
                }
            }
        }
        mLastPath = path;
        
        if(new_path) {
            LOG_INFO("New path received");
             mpMotionPlanningLibraries->printPathInWorld();
             
            _waypoints.write(path);

            std::vector<base::Trajectory> vec_traj = 
                    mpMotionPlanningLibraries->getTrajectoryInWorld(_trajectory_speed.get());
            _trajectory.write(vec_traj);
            
            std::vector<struct State> states = mpMotionPlanningLibraries->getStatesInWorld();
            _states.write(states); 
            
            _path_cost.write(cost);
        }
        base::Time t1 = base::Time::now();
        std::cout << "Time: Planner update hook executed in " << (t1-t0).toMilliseconds() << "ms." << std::endl;
    }
    
    // Export automatically generated SBPL motion primitives.
    // (will be generated in SBPL+XYTHETA if no mprim file has been specified.
    if(_config.get().mPlanningLibType == LIB_SBPL &&
        _config.get().mEnvType == ENV_XYTHETA &&
        _config.get().mSBPLEnvFile.empty()) {
        struct SbplMotionPrimitives mprims;
        if(mpMotionPlanningLibraries->getSbplMotionPrimitives(mprims)) {
            _sbpl_mprims_debug.write(mprims);
        }
    }
}

void Task::errorHook()
{
    TaskBase::errorHook();
}

void Task::stopHook()
{
    TaskBase::stopHook();
}

void Task::cleanupHook()
{
    TaskBase::cleanupHook();
}

bool Task::generateEscapeTrajectory() {
    if(mpMotionPlanningLibraries) {
        std::vector<base::Trajectory> escape_traj = 
                mpMotionPlanningLibraries->getEscapeTrajectoryInWorld();
        if(escape_traj.size() == 0) {
            LOG_WARN("Empty escape trajectory recieved");
            return false;
        }
        _escape_trajectory.write(escape_traj);
    }
    LOG_WARN("Motion planning library has not been created yet");
    return false;
}
