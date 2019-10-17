/* 
 * Copyright (c) 2018-2019, CNRS-UM LIRMM
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <mutex>
#include <thread>

#include <ros/ros.h>
#include <tf2_ros/transform_broadcaster.h>
#include <visualization_msgs/MarkerArray.h>

#include <mc_control/api.h>
#include <mc_control/fsm/Controller.h>
#include <mc_control/mc_controller.h>
#include <mc_rtc/logging.h>
#include <mc_rtc/ros.h>

#include <capture_walking/CaptureProblem.h>
#include <capture_walking/Contact.h>
#include <capture_walking/FloatingBaseObserver.h>
#include <capture_walking/FootstepPlan.h>
#include <capture_walking/HorizontalMPCProblem.h>
#include <capture_walking/Pendulum.h>
#include <capture_walking/PendulumObserver.h>
#include <capture_walking/Preview.h>
#include <capture_walking/Sole.h>
#include <capture_walking/Stabilizer.h>
#include <capture_walking/defs.h>
#include <capture_walking/utils/LowPassVelocityFilter.h>
#include <capture_walking/utils/clamp.h>
#include <capture_walking/utils/rotations.h>

namespace capture_walking
{
  /** Walking pattern generation methods.
   *
   */
  enum class WalkingPatternGeneration
  {
    CaptureProblem,
    HorizontalMPC
  };

  /** Capturability-based walking controller.
   *
   */
  struct MC_CONTROL_DLLAPI Controller : public mc_control::fsm::Controller
  {
    /** Initialization of the controller. 
     *
     * \param robot Robot model.
     *
     * \param dt Control timestep.
     *
     * \param config Configuration dictionary.
     *
     * Don't forget to fill reset() as well, as the controller might be
     * loaded/reloaded by mc_rtc.
     *
     */
    Controller(std::shared_ptr<mc_rbdyn::RobotModule> robot, double dt, const mc_rtc::Configuration & config);

    /** Reset the controller.
     *
     */
    //void reset(const mc_control::ControllerResetData & reset_data) override;

    /** Update robot mass estimate in all components.
     *
     * \param mass New mass estimate.
     *
     */
    void updateRobotMass(double mass);

    /** Reset robot to its initial (half-sitting) configuration.
     *
     * The reason why I do it inside the controller rather than via the current
     * mc_rtc way (switching to half_sitting controller then back to this one)
     * is <https://gite.lirmm.fr/multi-contact/mc_rtc/issues/54>.
     *
     */
    void internalReset();

    /** Main function of the controller, called at every control cycle.
     *
     */
    virtual bool run() override;

    /** Load footstep plan from configuration.
     *
     * \param name Plan name.
     *
     */
    void loadFootstepPlan(std::string name);

    /** Net contact wrench as measured by foot force sensors.
     *
     */
    sva::ForceVecd measuredContactWrench();

    /** This getter is only used for consistency with the rest of mc_rtc.
     *
     */
    Pendulum & pendulum()
    {
      return pendulum_;
    }

    /** Get control robot state.
     *
     */
    mc_rbdyn::Robot & controlRobot()
    {
      return mc_control::fsm::Controller::robot();
    }

    /** This getter is only used for consistency with the rest of mc_rtc.
     *
     */
    PendulumObserver & pendulumObserver()
    {
      return pendulumObserver_;
    }

    /** Get observed robot state.
     *
     */
    mc_rbdyn::Robot & realRobot()
    {
      return real_robots->robot();
    }

    /** This getter is only used for consistency with the rest of mc_rtc.
     *
     */
    Stabilizer & stabilizer()
    {
      return stabilizer_;
    }

    /** Update capturability preview.
     *
     */
    bool updatePreviewCPS();

    /** Update horizontal MPC preview.
     *
     */
    bool updatePreviewHMPC();

    /** Get fraction of total weight that should be sustained by the left foot.
     *
     */
    double leftFootRatio()
    {
      return leftFootRatio_;
    }

    /** Set fraction of total weight that should be sustained by the left foot.
     *
     * \param ratio Number between 0 and 1.
     *
     */
    void leftFootRatio(double ratio);

    /** Estimate left foot pressure ratio from force sensors.
     *
     */
    inline double measuredLeftFootRatio()
    {
      double leftFootPressure = realRobot().forceSensor("LeftFootForceSensor").force().z();
      double rightFootPressure = realRobot().forceSensor("RightFootForceSensor").force().z();
      leftFootPressure = std::max(0., leftFootPressure);
      rightFootPressure = std::max(0., rightFootPressure);
      return leftFootPressure / (leftFootPressure + rightFootPressure);
    }

    /** Get next double support duration.
     *
     */
    inline double doubleSupportDuration()
    {
      double duration;
      if (doubleSupportDurationOverride_ > 0.)
      {
        duration = doubleSupportDurationOverride_;
        doubleSupportDurationOverride_ = -1.;
      }
      else
      {
        duration = plan.doubleSupportDuration();
      }
      return duration;
    }

    /** Get next contact in plan.
     *
     */
    inline const Contact & nextContact()
    {
      return plan.nextContact();
    }

    /** Override next DSP duration.
     *
     * \param duration Custom DSP duration.
     *
     */
    inline void nextDoubleSupportDuration(double duration)
    {
      doubleSupportDurationOverride_ = duration;
    }

    /** Get previous contact in plan.
     *
     */
    inline const Contact & prevContact()
    {
      return plan.prevContact();
    }

    /** Get next SSP duration.
     *
     */
    inline double singleSupportDuration()
    {
      return plan.singleSupportDuration();
    }

    /** Get current support contact.
     *
     */
    inline const Contact & supportContact()
    {
      return plan.supportContact();
    }

    /** Get current target contact.
     *
     */
    inline const Contact & targetContact()
    {
      return plan.targetContact();
    }

    /** True during the last step.
     *
     */
    inline bool isLastSSP()
    {
      return (targetContact().id > nextContact().id);
    }

    /** True after the last step.
     *
     */
    inline bool isLastDSP()
    {
      return (supportContact().id > targetContact().id);
    }

    /** List available contact plans.
     *
     */
    inline std::vector<std::string> availablePlans() const
    {
      return plans_.keys();
    }

    /** Start new log segment.
     *
     * \param label Segment label.
     *
     */
    void startLogSegment(const std::string & label);

    /** Stop current log segment.
     *
     */
    void stopLogSegment();

  public: /* visible to FSM states */
    CaptureProblem cps;
    FootstepPlan plan;
    HorizontalMPCProblem hmpc;
    Sole sole;
    WalkingPatternGeneration wpg = WalkingPatternGeneration::CaptureProblem;
    bool emergencyStop = false;
    bool pauseWalking = false;
    double previewUpdatePeriod = HorizontalMPC::SAMPLING_PERIOD;
    std::shared_ptr<Preview> preview;
    std::vector<std::vector<double>> halfSitPose;

  private: /* hidden from FSM states */
    Eigen::Vector3d controlCom_;
    Eigen::Vector3d controlComd_;
    Eigen::Vector3d realCom_;
    Eigen::Vector3d realComd_;
    LowPassVelocityFilter<Eigen::Vector3d> comVelFilter_;
    Pendulum pendulum_;
    PendulumObserver pendulumObserver_;
    Stabilizer stabilizer_;
    bool isInTheAir_ = false;
    bool leftFootRatioJumped_ = false;
    double ctlTime_ = 0.;
    double doubleSupportDurationOverride_ = -1.; // [s]
    double leftFootRatio_ = 0.5;
    double robotMass_ = 0.; // [kg]
    FloatingBaseObserver floatingBaseObserver_;
    mc_rtc::Configuration hmpcConfig_;
    mc_rtc::Configuration plans_;
    std::string segmentName_ = "";
    unsigned nbCPSFailures_ = 0;
    unsigned nbCPSUpdates_ = 0;
    unsigned nbHMPCFailures_ = 0;
    unsigned nbHMPCUpdates_ = 0;
    unsigned nbLogSegments_ = 100;

  private: /* ROS */
    visualization_msgs::Marker getArrowMarker(const std::string & frame_id, const Eigen::Vector3d & from, const Eigen::Vector3d & to, char color, double scale = 1.);
    visualization_msgs::Marker getCoPMarker(const std::string & surfaceName, char color);
    visualization_msgs::Marker getForceMarker(const std::string & surfaceName, char color);
    visualization_msgs::Marker getPointMarker(const std::string & frame_id, const Eigen::Vector2d & pos, char color, double scale = 1.);
    visualization_msgs::Marker getPointMarker(const std::string & frame_id, const Eigen::Vector3d & pos, char color, double scale = 1.);
    visualization_msgs::Marker getContactMarker(const std::string & frame_id, char color);
    visualization_msgs::MarkerArray getPendulumMarkerArray(const Pendulum & state, char color);
    void publishMarkers();
    void publishTransforms();
    void spinner();

  private: /* ROS */
    bool isSpinning_ = false;
    int nextMarkerId_ = 0;
    ros::Publisher extraPublisher_;
    ros::Publisher footstepPublisher_;
    ros::Publisher pendulumObserverPublisher_;
    ros::Publisher pendulumPublisher_;
    ros::Publisher sensorPublisher_;
    std::thread spinThread_;
    std::unique_ptr<tf2_ros::TransformBroadcaster> tfBroadcaster_;
    unsigned rosSeq_ = 0;
  };
}
