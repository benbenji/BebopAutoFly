#include "ros/ros.h"
#include <tf/transform_datatypes.h>
#include <tf/transform_listener.h>
#include "moveit_multi_dof_plans/TransformTrajectory.h"
#include "moveit_multi_dof_plans/InverseTransformTrajectory.h"

class TrajectoryTransformer
{
public:
  bool transformTrajectory(moveit_multi_dof_plans::TransformTrajectory::Request& req,
                                  moveit_multi_dof_plans::TransformTrajectory::Response& res)
  {
    moveit_msgs::RobotTrajectory robotTrajectoryMsg;
    trajectory_msgs::MultiDOFJointTrajectory input;

    input = req.trajectoryToTransform.multi_dof_joint_trajectory;
    if (input.points.size() <= 0)
    {
	ROS_ERROR("Trajectory transformer recieved invalid input.");
	return false;
    }

    robotTrajectoryMsg.multi_dof_joint_trajectory = transform(input, false);
    res.transformedTrajectory = robotTrajectoryMsg;

    return true;
  }

  bool inverseTransformTrajectory(moveit_multi_dof_plans::TransformTrajectory::Request& req,
                                  moveit_multi_dof_plans::TransformTrajectory::Response& res)
  {
    moveit_msgs::RobotTrajectory robotTrajectoryMsg;
    trajectory_msgs::MultiDOFJointTrajectory input;

    input = req.trajectoryToTransform.multi_dof_joint_trajectory;
    if (input.points.size() <= 0)
    {
	ROS_ERROR("Inverse trajectory transformer recieved invalid input.");
	return false;
    }

    robotTrajectoryMsg.multi_dof_joint_trajectory = transform(input, true);
    res.transformedTrajectory = robotTrajectoryMsg;

    return true;
  }

  TrajectoryTransformer(ros::NodeHandle n)
  {
    n.param<std::string>("odom_frame_id", odom_frame_id, "odom");
    n.param<std::string>("base_frame_id", base_frame_id, "base_link");
  }

  bool getTf()
  {
    tf::TransformListener listener;
    tf::StampedTransform transform;

    try
    {
      ros::Time now = ros::Time::now();
      listener.waitForTransform(base_frame_id, odom_frame_id,
                              now, ros::Duration(3.0));
      listener.lookupTransform(base_frame_id, odom_frame_id,
                              now, transform);
    }
    catch (tf::TransformException ex)
    {
      ROS_ERROR("%s",ex.what());
    }

    initialRotation = transform.getRotation();

    ROS_INFO("The initial rotation between %s and %s is %f degrees at axis [%f,%f,%f]", base_frame_id.c_str(), odom_frame_id.c_str(),
		initialRotation.getAngle(), initialRotation.getAxis().getX(), initialRotation.getAxis().getY(), initialRotation.getAxis().getZ());
  }

private:

  tf::Quaternion initialRotation;
  std::string odom_frame_id;
  std::string base_frame_id;

  trajectory_msgs::MultiDOFJointTrajectory transform(trajectory_msgs::MultiDOFJointTrajectory input, bool inverse)
  {
    trajectory_msgs::MultiDOFJointTrajectory multi;

    multi.header = input.header;
    multi.joint_names = input.joint_names;

    for(int i = 0; i < input.points.size(); i++)
    {
	multi.points.push_back(getTransformedPoint(input.points[i], inverse));
    }

    return multi;
  }

  trajectory_msgs::MultiDOFJointTrajectoryPoint getTransformedPoint(trajectory_msgs::MultiDOFJointTrajectoryPoint input,
									bool inverse)
  {
     trajectory_msgs::MultiDOFJointTrajectoryPoint toReturn;
     toReturn.time_from_start = input.time_from_start;

     tf::Quaternion rotation = initialRotation;
     if (inverse)
     {
	rotation = initialRotation.inverse();
     }

     geometry_msgs::Transform transform;
     tf::Vector3 toRotate(input.transforms[0].translation.x,input.transforms[0].translation.y,input.transforms[0].translation.z);
     toRotate = toRotate.rotate(rotation.getAxis(), rotation.getAngle());

     transform.translation.x = toRotate.getX();
     transform.translation.y = toRotate.getY();
     transform.translation.z = toRotate.getZ();
     tf::Quaternion quaternionToRotate;
     tf::quaternionMsgToTF(input.transforms[0].rotation, quaternionToRotate);
     quaternionToRotate *= rotation;
     tf::quaternionTFToMsg(quaternionToRotate, transform.rotation);

     toReturn.transforms.push_back(transform);

     return toReturn;
  }
};

int main(int argc, char **argv)
{
  ros::init(argc, argv, "robot_trajectory_transformer");
  ros::NodeHandle n;

  TrajectoryTransformer server(n);
  server.getTf();
  ros::ServiceServer ss1 = n.advertiseService("robot_trajectory_transform", &TrajectoryTransformer::transformTrajectory, &server);
  ros::ServiceServer ss2 = n.advertiseService("inverse_robot_trajectory_transform", &TrajectoryTransformer::inverseTransformTrajectory, &server);

  ros::spin();

  return 0;
}