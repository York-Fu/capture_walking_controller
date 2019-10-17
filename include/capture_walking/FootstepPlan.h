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

#include <string>

#include <capture_walking/Contact.h>
#include <capture_walking/HorizontalMPC.h>
#include <capture_walking/Sole.h>
#include <capture_walking/utils/clamp.h>

namespace capture_walking
{
  /** Sequence of footsteps with gait parameters.
   *
   */
  struct FootstepPlan
  {
    /** Load plan from configuration dictionary.
     *
     * \param config Configuration dictionary.
     *
     */
    void load(const mc_rtc::Configuration & config);

    /** Save plan to configuration  dictionary.
     *
     * \param config Configuration dictionary.
     *
     */
    void save(mc_rtc::Configuration & config) const;

    /** Complete contacts from sole parameters.
     *
     * \param sole Sole parameters.
     *
     */
    void complete(const Sole & sole);

    /** Rewind plan to a given contact.
     *
     * \param startIndex Index of first support contact.
     *
     */
    void reset(unsigned startIndex = 0);

    /** Advance to next footstep in plan.
     *
     */
    void goToNextFootstep();

    /** Rewind one footstep back in plan.
     *
     * \note This function cannot rewind more than one step. It is only used
     * when activating a DoubleSupport to Standing transition.
     */
    void restorePreviousFootstep();

    /** Advance to next footstep in plan, taking into account drift in reaching
     * target contact.
     *
     * \param actualTargetPose Actual target pose.
     *
     */
    void goToNextFootstep(const sva::PTransformd & actualTargetPose);

    /** Compute initial floating-base transform over first contact.
     *
     * \param robot Robot model.
     *
     */
    sva::PTransformd computeInitialTransform(const mc_rbdyn::Robot & robot) const;

    /** Default CoM height.
     *
     */
    inline double comHeight() const
    {
      return comHeight_;
    }

    /** Set default CoM height.
     *
     */
    inline void comHeight(double height)
    {
      constexpr double MIN_COM_HEIGHT = 0.7; // [m]
      constexpr double MAX_COM_HEIGHT = 0.85; // [m]
      comHeight_ = clamp(height, MIN_COM_HEIGHT, MAX_COM_HEIGHT);
    }

    /** Reference to list of contacts.
     *
     */
    inline const std::vector<Contact> & contacts() const
    {
      return contacts_;
    }

    /** Default double-support duration.
     *
     */
    inline double doubleSupportDuration() const
    {
      return doubleSupportDuration_;
    }

    /** Set default double-support duration.
     *
     */
    inline void doubleSupportDuration(double duration)
    {
      constexpr double MIN_DS_DURATION = 0.;
      constexpr double MAX_DS_DURATION = 1.;
      constexpr double T = HorizontalMPC::SAMPLING_PERIOD;
      duration = std::round(duration / T) * T;
      doubleSupportDuration_ = clamp(duration, MIN_DS_DURATION, MAX_DS_DURATION);
    }

    /** Get final double support phase duration.
     *
     */
    inline double finalDSPDuration() const
    {
      return finalDSPDuration_;
    }

    /** Set final double support phase duration.
     *
     */

    inline void finalDSPDuration(double duration)
    {
      finalDSPDuration_ = clamp(duration, 0., 1.6);
    }

    /** Get initial double support phase duration.
     *
     */
    inline double initDSPDuration() const
    {
      return initDSPDuration_;
    }

    /** Set initial double support phase duration.
     *
     */

    inline void initDSPDuration(double duration)
    {
      initDSPDuration_ = clamp(duration, 0., 1.6);
    }

    /** Get swing foot landing pitch angle.
     *
     */
    inline double landingPitch() const
    {
      if (prevContact_.swingConfig.has("landing_pitch"))
      {
        return prevContact_.swingConfig("landing_pitch");
      }
      return landingPitch_;
    }

    /** Set swing foot takeoff pitch angle.
     *
     */
    inline void landingPitch(double pitch)
    {
      constexpr double MIN_LANDING_PITCH = -1.;
      constexpr double MAX_LANDING_PITCH = 1.;
      landingPitch_ = clamp(pitch, MIN_LANDING_PITCH, MAX_LANDING_PITCH);
    }

    /** Get swing foot landing ratio.
     *
     */
    inline double landingRatio() const
    {
      if (supportContact_.swingConfig.has("landing_ratio"))
      {
        return supportContact_.swingConfig("landing_ratio");
      }
      return landingRatio_;
    }

    /** Set swing foot landing ratio.
     *
     * \param ratio New ratio.
     *
     */
    inline void landingRatio(double ratio)
    {
      landingRatio_ = clamp(ratio, 0., 0.5);
    }

    /** Next contact in plan.
     *
     */
    inline const Contact & nextContact() const
    {
      return nextContact_;
    }

    /** Previous contact in plan.
     *
     */
    inline const Contact & prevContact() const
    {
      return prevContact_;
    }

    /** Default single-support duration.
     *
     */
    inline double singleSupportDuration() const
    {
      return singleSupportDuration_;
    }

    /** Set single-support duration.
     *
     */
    inline void singleSupportDuration(double duration)
    {
      constexpr double MIN_SS_DURATION = 0.;
      constexpr double MAX_SS_DURATION = 2.;
      constexpr double T = HorizontalMPC::SAMPLING_PERIOD;
      duration = std::round(duration / T) * T;
      singleSupportDuration_ = clamp(duration, MIN_SS_DURATION, MAX_SS_DURATION);
    }

    /** Current support contact.
     *
     */
    inline const Contact & supportContact() const
    {
      return supportContact_;
    }

    /** Default swing-foot height.
     *
     */
    inline double swingHeight() const
    {
      if (prevContact_.swingConfig.has("height"))
      {
        return prevContact_.swingConfig("height");
      }
      return swingHeight_;
    }

    /** Set default swing-foot height.
     *
     */
    inline void swingHeight(double height)
    {
      constexpr double MIN_SWING_FOOT_HEIGHT = 0.;
      constexpr double MAX_SWING_FOOT_HEIGHT = 0.25;
      swingHeight_ = clamp(height, MIN_SWING_FOOT_HEIGHT, MAX_SWING_FOOT_HEIGHT);
    }

    /** Get swing foot takeoff offset.
     *
     */
    inline Eigen::Vector3d takeoffOffset() const
    {
      if (prevContact_.swingConfig.has("takeoff_offset"))
      {
        return prevContact_.swingConfig("takeoff_offset");
      }
      return takeoffOffset_;
    }

    /** Set swing foot takeoff offset.
     *
     */
    inline void takeoffOffset(const Eigen::Vector3d & offset)
    {
      takeoffOffset_ = offset;
    }

    /** Get swing foot takeoff pitch angle.
     *
     */
    inline double takeoffPitch() const
    {
      if (prevContact_.swingConfig.has("takeoff_pitch"))
      {
        return prevContact_.swingConfig("takeoff_pitch");
      }
      return takeoffPitch_;
    }

    /** Set swing foot takeoff pitch angle.
     *
     */
    inline void takeoffPitch(double pitch)
    {
      constexpr double MIN_TAKEOFF_PITCH = -1.;
      constexpr double MAX_TAKEOFF_PITCH = 1.;
      takeoffPitch_ = clamp(pitch, MIN_TAKEOFF_PITCH, MAX_TAKEOFF_PITCH);
    }

    /** Current target contact.
     *
     */
    inline const Contact & targetContact() const
    {
      return targetContact_;
    }

    /** Get swing foot takeoff ratio.
     *
     */
    inline double takeoffRatio() const
    {
      if (supportContact_.swingConfig.has("takeoff_ratio"))
      {
        return supportContact_.swingConfig("takeoff_ratio");
      }
      return takeoffRatio_;
    }

    /** Set swing foot takeoff ratio.
     *
     * \param ratio New ratio.
     *
     */
    inline void takeoffRatio(double ratio)
    {
      takeoffRatio_ = clamp(ratio, 0., 0.5);
    }

  public:
    std::string name = "";

  private:
    Contact nextContact_;
    Contact prevContact_;
    Contact supportContact_;
    Contact targetContact_;
    Eigen::Vector3d takeoffOffset_ = Eigen::Vector3d::Zero();
    double comHeight_ = 0.78; // [m]
    double doubleSupportDuration_ = 0.2; // [s]
    double finalDSPDuration_ = 0.6; // [s]
    double initDSPDuration_ = 0.6; // [s]
    double landingPitch_ = 0.;
    double landingRatio_ = 0.05;
    double singleSupportDuration_ = 0.8; // [s]
    double swingHeight_ = 0.04; // [m]
    double takeoffPitch_ = 0.;
    double takeoffRatio_ = 0.05;
    std::vector<Contact> contacts_;
    unsigned nextFootstep_ = 0;
  };
}

namespace mc_rtc
{
  template<>
  struct ConfigurationLoader<capture_walking::FootstepPlan>
  {
    static capture_walking::FootstepPlan load(const mc_rtc::Configuration & config)
    {
      capture_walking::FootstepPlan plan;
      plan.load(config);
      return plan;
    }

    static mc_rtc::Configuration save(const capture_walking::FootstepPlan & plan)
    {
      mc_rtc::Configuration config;
      plan.save(config);
      return config;
    }
  };
}
