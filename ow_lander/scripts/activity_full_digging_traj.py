#!/usr/bin/env python

# __BEGIN_LICENSE__
# Copyright (c) 2018-2019, United States Government as represented by the
# Administrator of the National Aeronautics and Space Administration. All
# rights reserved.
# __END_LICENSE__

import rospy
import constants
import math
import copy
from tf.transformations import quaternion_from_euler
from utils import is_shou_yaw_goal_in_range

def arg_parsing_lin(req):
  if req.use_defaults :
    # Default trenching values
    trench_x=1.5
    trench_y=0
    trench_d=0.02
    length=0.3
    delete_prev_traj=False

  else :
    trench_x=req.trench_x
    trench_y=req.trench_y
    trench_d=req.trench_d
    length=req.length
    delete_prev_traj=req.delete_prev_traj

  return [req.use_defaults,trench_x,trench_y,trench_d,length,delete_prev_traj]

def arg_parsing_circ(req):
  if req.use_defaults :
    # Default trenching values
    trench_x=1.5
    trench_y=0
    trench_d=0.02
    delete_prev_traj=False

  else :
    trench_x=req.trench_x
    trench_y=req.trench_y
    trench_d=req.trench_d
    delete_prev_traj=req.delete_prev_traj

  return [req.use_defaults,trench_x,trench_y,trench_d,delete_prev_traj]


def move_to_pre_trench_configuration(move_arm, x_tr, y_tr):
  # Compute shoulder yaw angle to trench
  alpha = math.atan2(y_tr-constants.Y_SHOU, x_tr-constants.X_SHOU)
  h = math.sqrt( pow(y_tr-constants.Y_SHOU,2) + pow(x_tr-constants.X_SHOU,2) )
  l = constants.Y_SHOU - constants.HAND_Y_OFFSET
  beta = math.asin (l/h)
    # Move to pre trench position, align shoulder yaw
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_DIST_PITCH] = 0
  joint_goal[constants.J_HAND_YAW] = math.pi/2.2
  joint_goal[constants.J_PROX_PITCH] = -math.pi/2
  joint_goal[constants.J_SHOU_PITCH] = math.pi/2
  joint_goal[constants.J_SHOU_YAW] = alpha + beta

  # If out of joint range, abort
  if (is_shou_yaw_goal_in_range(joint_goal) == False):
    return False

  joint_goal[constants.J_SCOOP_YAW] = 0
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()
  return True


def plan_cartesian_path_lin(move_arm, length):

  waypoints = []
  wpose = move_arm.get_current_pose().pose
  wpose.position.x += length # Second move forward/backwards in (x)
  waypoints.append(copy.deepcopy(wpose))

  (plan, fraction) = move_arm.compute_cartesian_path(
                               waypoints,   # waypoints to follow
                               0.01,        # eef_step
                               0.0)         # jump_threshold
  #ROS_INFO("tutorial", "Visualizing plan 4 (Cartesian path) (%.2f%% acheived)", fraction * 100.0);
  # Note: We are just planning, not asking move_group to actually move the robot yet:
  return plan, fraction

def dig_linear_trench(move_arm,move_limbs,x_tr, y_tr, depth, length):

  pre_move_complete = move_to_pre_trench_configuration(move_arm, x_tr, y_tr)
  if pre_move_complete == False:
    return False
  ## Rotate hand yaw to dig in
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_HAND_YAW] = 0
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()

  #rotate scoop
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_SCOOP_YAW] = math.pi/2
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()

  #rotate dist pith to pre-trenching position.
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_DIST_PITCH] = -math.pi/2
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()

  ## Once aligned to trench goal, place hand above trench middle point
  goal_pose = move_limbs.get_current_pose().pose
  goal_pose.position.x = x_tr
  goal_pose.position.y = y_tr
  goal_pose.position.z = constants.GROUND_POSITION + constants.SCOOP_OFFSET - depth
  move_limbs.set_pose_target(goal_pose)
  plan = move_limbs.plan()

  if len(plan.joint_trajectory.points) == 0: # If no plan found, abort
    return False

  plan = move_limbs.go(wait=True)
  move_limbs.stop()
  move_limbs.clear_pose_targets()

  #  rotate to dig in the ground
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_DIST_PITCH] = 55.0/180.0*math.pi # scoop parallel to ground
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()

  # linear trenching
  cartesian_plan, fraction = plan_cartesian_path_lin(move_arm, length)
  move_arm.execute(cartesian_plan, wait=True)
  move_arm.stop()

  #  rotate to dig out
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_DIST_PITCH] = math.pi/2
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()

  return True

def dig_trench(move_arm,move_limbs,x_tr, y_tr, depth):

  pre_move_complete = move_to_pre_trench_configuration(move_arm, x_tr, y_tr)
  if pre_move_complete == False:
    return False

  # Once aligned to trench goal, place hand above trench middle point
  goal_pose = move_limbs.get_current_pose().pose
  goal_pose.position.x = x_tr
  goal_pose.position.y = y_tr
  goal_pose.position.z = constants.GROUND_POSITION + constants.SCOOP_OFFSET - depth
  move_limbs.set_pose_target(goal_pose)
  plan = move_limbs.plan()

  if len(plan.joint_trajectory.points) == 0: # If no plan found, abort
    return False

  plan = move_limbs.go(wait=True)
  move_limbs.stop()
  move_limbs.clear_pose_targets()

  # Rotate hand yaw to dig in
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_HAND_YAW] = 0
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()

  # Insert here for linear trenching

  # Rotate hand yaw to dig out
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_HAND_YAW] = -math.pi/2.2
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()

  # # Go back to safe position and align yaw to deliver
  # joint_goal = move_arm.get_current_joint_values()
  # joint_goal[constants.J_DIST_PITCH] = 0
  # joint_goal[constants.J_HAND_YAW] = -math.pi/2
  # joint_goal[constants.J_PROX_PITCH] = -math.pi/2
  # joint_goal[constants.J_SHOU_PITCH] = math.pi/2
  # joint_goal[constants.J_SHOU_YAW] = constants.SHOU_YAW_DELIV
  # joint_goal[constants.J_SCOOP_YAW]= 0
  # move_arm.go(joint_goal, wait=True)
  # move_arm.stop()

  # # Go to deliver position
  # joint_goal = move_arm.get_current_joint_values()
  # joint_goal[constants.J_PROX_PITCH]= math.pi/2 - 0.1
  # joint_goal[constants.J_SCOOP_YAW]= math.pi - 0.05
  # move_arm.go(joint_goal, wait=True)
  # move_arm.stop()

  # # Deliver (high amplitude)
  # joint_goal = move_arm.get_current_joint_values()
  # joint_goal[constants.J_HAND_YAW] = -math.pi
  # move_arm.go(joint_goal, wait=True)
  # move_arm.stop()
  # joint_goal[constants.J_HAND_YAW] = math.pi/2
  # move_arm.go(joint_goal, wait=True)
  # move_arm.stop()
  return True

def go_home(move_arm):
  # Move to home position
  joint_goal = move_arm.get_current_joint_values()
  joint_goal[constants.J_SHOU_YAW] = rospy.get_param('/stowed_shou_yaw', default=0)
  joint_goal[constants.J_SHOU_PITCH] = rospy.get_param('/stowed_shou_pitch', default=0)
  joint_goal[constants.J_PROX_PITCH] = rospy.get_param('/stowed_prox_pitch', default=0)
  joint_goal[constants.J_DIST_PITCH] = rospy.get_param('/stowed_dist_pitch', default=0)
  joint_goal[constants.J_HAND_YAW] = rospy.get_param('/stowed_hand_yaw', default=0)
  joint_goal[constants.J_SCOOP_YAW] = rospy.get_param('/stowed_scoop_yaw', default=0)
  move_arm.go(joint_goal, wait=True)
  move_arm.stop()
