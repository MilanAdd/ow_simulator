// The Notices and Disclaimers for Ocean Worlds Autonomy Testbed for Exploration
// Research and Simulation can be found in README.md in the root directory of
// this repository.

// OW power system ROS node - publishes values from a csv generated by the matlab
// battery model as a placeholder for once battery models are linked.


#include <chrono>

#include <ros/ros.h>
#include <ros/package.h>
#include <ros/console.h>
#include <std_msgs/Int16.h>
#include <std_msgs/Float64.h>

#include "power_system_util.h"

using namespace std;

int main(int argc, char* argv[]) {
  using namespace std::chrono;
  ros::init(argc,argv,"power_system_node");
  ros::NodeHandle nh ("power_system_node");

  //Construct our State of Charge (SOC) publisher
  ros::Publisher SOC_pub = nh.advertise<std_msgs::Float64>("state_of_charge",1000);
  //Construct our Remaining Useful Life (RUL) publisher (Seconds)
  ros::Publisher RUL_pub = nh.advertise<std_msgs::Int16>("remaining_useful_life",1000);
  //Construct our Battery Temperature (TempBat) publisher
  ros::Publisher TempBat_pub = nh.advertise<std_msgs::Float64>("battery_temperature",1000);
  
  //Load power values csv
  string csv_path;
  string default_csv = "/data/data_const_load.csv";
  bool csv_path_param_exist = nh.param("power_draw_csv_path", csv_path,
    ros::package::getPath("ow_power_system") + default_csv);

  if (!csv_path_param_exist)
  {
    ROS_WARN_NAMED("power_system_node", "power_draw_csv_path param was not set! Using default value: data_const_load.csv");
  }

  ROS_INFO_STREAM_NAMED("power_system_node", "power_draw_csv_path is set to: " << csv_path);

  // Read battery data from a file.
  auto power_data = read_file(csv_path);

  // Create a configuration from a file
  string config_path = ros::package::getPath("ow_power_system")+"/config/example.cfg";
  ConfigMap config(config_path);
  // Contruct a new prognoser using the prognoser factory. The prognoser
  // will automatically construct an appropriate model, observer and predictor
  // based on the values specified in the config.
  auto prognoser = PrognoserFactory::instance().Create("ModelBasedPrognoser", config);

  // Retrieve our publication rate expressed in Hz
  double power_update_rate = 1;
  double power_update_rate_override;  // allow the user to override it
  bool update_rate_param_exist = nh.param("power_update_rate", power_update_rate_override, 0.1);

  if (update_rate_param_exist)
  {
    ROS_WARN_NAMED("power_system_node", "Overriding the default power_update_rate!");
    // Validated the parameter
    if (power_update_rate == 0)
    {
      ROS_ERROR_NAMED("power_system_node", "power_update_rate param was set to zero! passed value ignored.");
    }
    else
    {
      power_update_rate = power_update_rate_override;
    }
  }

  ROS_INFO_STREAM_NAMED("power_system_node", "power_update_rate is set to: " << power_update_rate << " Hz");
  
  // ROS Loop. Note that once this loop starts,
  // this function (and node) is terminated with an interrupt.
  
  ros::Rate rate(power_update_rate);
  //individual soc_msg to be published by SOC_pub
  std_msgs::Float64 soc_msg;
  std_msgs::Int16 rul_msg;
  std_msgs::Float64 tempbat_msg;
  ROS_INFO ("Power system node running");

  int count = 0;
  
  while (ros::ok()) {

    // For each line of data in the example file, run a single prediction step.
    for (const auto & line : power_data) {
      // Get a new prediction
      auto prediction = prognoser->step(line);
      // Get the event for battery EoD. The first line of data is used to initialize the observer,
      // so the first prediction won't have any events.
      if (prediction.getEvents().size() == 0) {
          continue;
      }
      auto eod_event = prediction.getEvents().front();
      // The time of event is a `UData` structure, which represents a data
      // point while maintaining uncertainty. For the MonteCarlo predictor
      // used by this example, the uncertainty is captured by storing the
      // result of each particle used in the prediction.
      UData eod_time = eod_event.getTOE();
      if (eod_time.uncertainty() != UType::Samples) {
        ROS_WARN_NAMED("power_system_node", "Unexpected uncertainty type for EoD prediction");
        if (count > 0) {
          RUL_pub.publish(rul_msg);
          //SOC_pub.publish(soc_msg);
          //TempBat_pub.publish(tempbat_msg);
        } else {
          rul_msg.data = 0;
          //soc_msg.data = 0.0;
          //TempBat_msg.data = 0.0;
          RUL_pub.publish(rul_msg);
          //SOC_pub.publish(soc_msg);
          //TempBat_pub.publish(tempbat_msg);
        }
        ros::spinOnce();
        rate.sleep();
        continue;
      }
      
      // For this example, we will print the median EoD.
      auto samples = eod_time.getVec();
      std::sort(samples.begin(), samples.end());
      double eod_median = samples.at(samples.size() / 2);
      auto now =  MessageClock::now();
      auto now_s = duration_cast<std::chrono::seconds>(now.time_since_epoch());
      double rul_median = eod_median - now_s.count();
      rul_msg.data = rul_median;

      // State of Charge Code
      //UData currentSOC = eod_event.getState()[0];
      // For this example, we will print the median SOC
      //auto samples2 = currentSOC.getVec();
      //std::sort(samples2.begin(), samples2.end());
      //double soc_median = samples2.at(samples2.size() / 2);
      //soc_msg.data = soc_median;

      // Temperature Code
      //std::vector<UData> systemStates = eod_event.getSystemState()[0]; // Get the system (battery) state at t=0 (now)
      // This is in format [stateId][sampleId].
      // Here you will have to choose the sampleId the corresponds to your chosen percentile (e.g., Median) - and load it into a vector<double> where each element is a different state (in same order as above). 
      // For Example:
      //std::vector<double> medianSystemState(systemStates.size());
      //for (int i = 0; i < medianSystemState.size(); i++) 
        //medianSystemState[i] = systemStates[i][MEDIAN]; // Where MEDIAN corresponds to the sampleID of the median EOL prediction
        
      // Next, You will pass it into the model outputEqn to get the outputs:
      //auto output = model.outputEqn(medianSystemState);
      //double temperature = output[1];
      //tempbat_msg.data = temperature;
      
      //publish current SOC & RUL
      //SOC_pub.publish(soc_msg);
      RUL_pub.publish(rul_msg);
      //TempBat_pub.publish(tempbat_msg);
      count+=1;
      ros::spinOnce();
      rate.sleep();
    }
        
  }
  return 0;
}