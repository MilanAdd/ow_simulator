// The Notices and Disclaimers for Ocean Worlds Autonomy Testbed for Exploration
// Research and Simulation can be found in README.md in the root directory of
// this repository.

#include "ow_faults_injection/FaultInjector.h"
#include <algorithm>

using namespace ow_lander;

using std::string;
// static constexpr float64 SET_FLAG = 1.0;

FaultInjector::FaultInjector(ros::NodeHandle& nh)
{ 
  string original_str = "/_original";
  //  arm pub and subs
  const char* joint_states_str = "/joint_states";
  m_joint_state_sub = nh.subscribe( original_str + joint_states_str, 
                                    10,
                                    &FaultInjector::jointStateCb, 
                                    this);
  m_joint_state_remapped_pub = nh.advertise<sensor_msgs::JointState>(joint_states_str, 10);
  m_joint_state_flags_pub = nh.advertise<ow_faults_detection::JointStatesFlag>(string("/flags") + joint_states_str, 10);

  const char* ft_sensor_dist_pitch_str = "/ft_sensor_dist_pitch";
  m_dist_pitch_ft_sensor_sub = nh.subscribe( original_str + ft_sensor_dist_pitch_str,
                                             10, 
                                             &FaultInjector::distPitchFtSensorCb, 
                                             this);
  m_dist_pitch_ft_sensor_pub = nh.advertise<geometry_msgs::WrenchStamped>(ft_sensor_dist_pitch_str, 10);

  //camera sub and repub for remapped topic
  const char* image_raw_str = "/StereoCamera/left/image_raw";
  m_camera_raw_sub = nh.subscribe( original_str + image_raw_str,
                                   10, 
                                   &FaultInjector::cameraFaultRepublishCb, 
                                   this);
  m_camera_trigger_remapped_pub = nh.advertise<sensor_msgs::Image>(image_raw_str, 10);

  srand (static_cast <unsigned> (time(0)));
}

void FaultInjector::faultsConfigCb(ow_faults_injection::FaultsConfig& faults, uint32_t level)
{
  // This is where we would check to see if faults is different from m_faults
  // if we wanted to change some state based on that.

  // Store current set of faults for later use
  m_faults = faults;
}

void FaultInjector::cameraFaultRepublishCb(const sensor_msgs::Image& msg)
{
  if (!m_cam_fault) {// if no fault
    m_camera_trigger_remapped_pub.publish(msg);
  } 
}

void FaultInjector::jointStateCb(const sensor_msgs::JointStateConstPtr& msg)
{
  ow_faults_detection::JointStatesFlag flag_msg;
  flag_msg.header.stamp = ros::Time::now();
  flag_msg.header.frame_id = "world";

  sensor_msgs::JointState output(*msg);
  flag_msg.name = output.name;
  flag_msg.flags = output.effort;

  // Populate the map once here.
  // This assumes the collection of joints will never change.
  if (m_joint_state_indices.empty()) {
    for (int j = 0; j < NUM_JOINTS; j ++) {
      int index = findPositionInGroup(msg->name, joint_names[j]);
      if (index >= 0)
        m_joint_state_indices.push_back(index);
    }
  }

  for (int i = 0; i < flag_msg.flags.size(); i ++){
    flag_msg.flags[i] = 0.0;
  }

  // Set failed sensor values to 0
  unsigned int index;

  // checking rqt
  checkCamFaults();
  const float SET_FLAG = 1;
  //ant faults
  if (m_faults.ant_pan_encoder_failure || m_faults.ant_pan_effort_failure 
      && findJointIndex(J_ANT_PAN, index)){
    flag_msg.flags[index] = SET_FLAG;
  }
  if ( m_faults.ant_tilt_encoder_failure || m_faults.ant_tilt_effort_failure 
      && findJointIndex(J_ANT_TILT, index)) {
    flag_msg.flags[index] = SET_FLAG;
  }

  //arm faults
  if (m_faults.shou_yaw_encoder_failure && findJointIndex(J_SHOU_YAW, index)) {
    output.position[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }
  if (m_faults.shou_yaw_effort_failure && findJointIndex(J_SHOU_YAW, index)) {
    output.effort[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }

  if (m_faults.shou_pitch_encoder_failure && findJointIndex(J_SHOU_PITCH, index)) {
    output.position[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }
  if (m_faults.shou_pitch_effort_failure && findJointIndex(J_SHOU_PITCH, index)) {
    output.effort[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }

  if (m_faults.prox_pitch_encoder_failure && findJointIndex(J_PROX_PITCH, index)) {
    output.position[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }
  if (m_faults.prox_pitch_effort_failure && findJointIndex(J_PROX_PITCH, index)) {
    output.effort[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }

  if (m_faults.dist_pitch_encoder_failure && findJointIndex(J_DIST_PITCH, index)) {
    output.position[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }
  if (m_faults.dist_pitch_effort_failure && findJointIndex(J_DIST_PITCH, index)) {
    output.effort[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }

  if (m_faults.hand_yaw_encoder_failure && findJointIndex(J_HAND_YAW, index)) {
    output.position[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }
  if (m_faults.hand_yaw_effort_failure && findJointIndex(J_HAND_YAW, index)) {
    output.effort[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }

  if (m_faults.scoop_yaw_encoder_failure && findJointIndex(J_SCOOP_YAW, index)) {
    output.position[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }
  if (m_faults.scoop_yaw_effort_failure && findJointIndex(J_SCOOP_YAW, index)) {
    output.effort[index]  = FAULT_ZERO_TELEMETRY;
    flag_msg.flags[index] = SET_FLAG;
  }

  m_joint_state_remapped_pub.publish(output);
  m_joint_state_flags_pub.publish(flag_msg);
}

void FaultInjector::distPitchFtSensorCb(const geometry_msgs::WrenchStamped& msg)
{
  if (!m_faults.groups.ft_sensor_faults.enable) {
    m_dist_pitch_ft_sensor_pub.publish(msg);
    return;
  }

  auto out_msg = msg;

  if (m_faults.groups.ft_sensor_faults.zero_signal_failure) {
    out_msg.wrench.force = geometry_msgs::Vector3();
    out_msg.wrench.torque = geometry_msgs::Vector3();
  }

  auto mean = m_faults.groups.ft_sensor_faults.signal_bias_failure;
  auto stddev = m_faults.groups.ft_sensor_faults.signal_noise_failure;
  // TODO: consider optimizing this by re-creating the distribution only when
  // mean and stddev values change
  auto normal_dist = std::normal_distribution<float>(mean, stddev);
  out_msg.wrench.force.x += normal_dist(m_random_generator);
  out_msg.wrench.force.y += normal_dist(m_random_generator);
  out_msg.wrench.force.z += normal_dist(m_random_generator);
  out_msg.wrench.torque.x += normal_dist(m_random_generator);
  out_msg.wrench.torque.y += normal_dist(m_random_generator);
  out_msg.wrench.torque.z += normal_dist(m_random_generator);

  m_dist_pitch_ft_sensor_pub.publish(out_msg);
}

void FaultInjector::checkCamFaults()
{
  m_cam_fault = m_faults.camera_left_trigger_failure;
}

int FaultInjector::findPositionInGroup(const std::vector<string>& group, const string& item)
{
  auto iter = std::find(group.begin(), group.end(), item);
  return (iter == group.end()) ? -1 :  iter - group.begin();
}

bool FaultInjector::findJointIndex(unsigned int joint, unsigned int& out_index)
{
  if(joint >= m_joint_state_indices.size())
    return false;

  out_index = m_joint_state_indices[joint];
  return true;
}
