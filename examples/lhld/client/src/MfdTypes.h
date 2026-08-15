/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Semantic input and projected-frame types for the reusable LHLD core.
 *
 * The types here are the integration boundary for the three MFD pages
 * (Radar/FCR, SMS, NAV/HSD). External avionics fill @ref MfdInputSample once per
 * rendered frame and pass it to @ref MfdController::Populate. The bundled
 * @ref MfdRadarSimulation is only one possible producer of that sample.
 */

#include <array>
#include <cstddef>

namespace lhld
{
/**
 * @defgroup lhld_integration LHLD integration API
 * @brief Semantic API used to drive the generated LHLD pages.
 * @{
 */

/** @brief Maximum number of radar tracks published by the demo sensor. */
inline constexpr std::size_t kMaxRadarTracks = 12U;
/** @brief Maximum number of authored steerpoint slots shown on the HSD flight plan. */
inline constexpr std::size_t kSteerpointCount = 8U;
/** @brief Number of weapon stations on the SMS stores diagram. */
inline constexpr std::size_t kStationCount = 9U;
/** @brief Number of option-select-button legends around one MFD page. */
inline constexpr std::size_t kOsbCount = 20U;

/** @brief Lightweight 2D vector expressed in MFD logical coordinates ([-1, 1]). */
struct MfdVec2
{
    /** @brief Horizontal MFD-space coordinate. */
    float x = 0.0f;
    /** @brief Vertical MFD-space coordinate, positive up. */
    float y = 0.0f;
};

/** @brief Aircraft master mode shared by the three MFD pages, as on the real jet. */
enum class MasterMode
{
    /** @brief Navigation master mode. */
    Nav,
    /** @brief Air-to-air master mode. */
    AirToAir,
    /** @brief Air-to-ground master mode. */
    AirToGround,
    /** @brief Dogfight override master mode. */
    Dogfight
};

/** @brief Which MFD page currently owns the option-select buttons. */
enum class MfdPage
{
    /** @brief Fire-control radar (FCR) B-scope page. */
    Radar,
    /** @brief Stores management (SMS) page. */
    Sms,
    /** @brief Horizontal situation display (HSD/NAV) page. */
    Nav,
    /** @brief Air-to-ground delivery profile page. */
    Ag
};

/** @brief Fire-control radar search/track submode. */
enum class RadarSubmode
{
    /** @brief Range-while-search. */
    Rws,
    /** @brief Track-while-scan. */
    Tws,
    /** @brief Single-target-track (bugged target). */
    Stt
};

/** @brief Fire-control-radar power and RF-emission state. */
enum class RadarOperatingState
{
    /** @brief Radar powered and radiating normally. */
    Operating,
    /** @brief Radar powered but prevented from radiating by RF silence. */
    Silent,
    /** @brief Fire-control radar power is off. */
    Off
};

/** @brief Quality of the latest sensor update associated with one radar track. */
enum class RadarTrackQuality
{
    /** @brief Position is backed by a current radar measurement. */
    Measured,
    /** @brief Position is extrapolated from the last valid radar measurement. */
    Extrapolated
};

/** @brief Radar-owned lifecycle state controlling the FCR symbol family. */
enum class RadarTrackState
{
    /** @brief RWS search return. */
    Search,
    /** @brief TWS trackfile maintained by the radar. */
    Trackfile,
    /** @brief Designated system target (bugged track). */
    SystemTarget,
    /** @brief Track maintained in single-target-track mode. */
    SingleTargetTrack
};

/** @brief Air-to-ground delivery format selected on the stores profile. */
enum class AirGroundDeliveryMode
{
    /** @brief Continuously computed impact point style delivery. */
    Ccip,
    /** @brief Continuously computed release point style delivery. */
    Ccrp
};

/**
 * @brief Aircraft state shared by every MFD page.
 */
struct OwnshipState
{
    /** @brief True heading in degrees, wrapped to [0, 360). */
    float headingDeg = 270.0f;
    /** @brief True airspeed in knots. */
    float speedKts = 420.0f;
    /** @brief Mach number. */
    float mach = 0.72f;
    /** @brief Barometric altitude in feet MSL. */
    float altitudeFt = 22000.0f;
};

/**
 * @brief One fire-control-radar track in sensor-native semantic units.
 *
 * Range, azimuth and elevation form the natural radar measurement boundary.
 * The projection derives B-scope coordinates and target altitude; callers do
 * not need to know authored page coordinates or duplicate projection math.
 * Track presence and lifecycle are authoritative sensor outputs: downstream
 * page code must not infer detection or loss from measurement geometry.
 */
struct RadarTrack
{
    /**
     * @brief True when the radar source publishes this track to the FCR.
     * @note Detection volume, radar power and track-loss decisions belong to
     * the input source. The projection and controller trust this value.
     */
    bool active = false;
    /** @brief Radar-owned state selecting the corresponding FCR symbol family. */
    RadarTrackState state = RadarTrackState::Search;
    /** @brief Slant range to the track in nautical miles. */
    float rangeNm = 20.0f;
    /** @brief Azimuth relative to aircraft boresight in degrees, positive right. */
    float azimuthDeg = 0.0f;
    /** @brief Elevation relative to aircraft boresight in degrees, positive up. */
    float elevationDeg = 0.0f;
    /** @brief Track ground heading in degrees. */
    float headingDeg = 90.0f;
    /** @brief Closure in knots; positive means the range is decreasing. */
    float closureKts = 0.0f;
    /** @brief Target aspect angle in degrees. */
    float aspectDeg = 0.0f;
    /** @brief Track ground speed in knots. */
    float speedKts = 0.0f;
    /** @brief True when the track is classified hostile. */
    bool hostile = true;
    /** @brief Whether the current position is measured or extrapolated. */
    RadarTrackQuality quality = RadarTrackQuality::Measured;
};

/**
 * @brief Operator radar controls, owned by the panel, not by the simulation.
 */
struct RadarSettings
{
    /** @brief Radar power/RF state controlling FCR OFF and NO RAD presentation. */
    RadarOperatingState operatingState = RadarOperatingState::Operating;
    /** @brief Active search/track submode. */
    RadarSubmode submode = RadarSubmode::Rws;
    /** @brief Range scale in nautical miles (10/20/40/80). */
    float rangeScaleNm = 40.0f;
    /** @brief Azimuth scan width in degrees (30/60). */
    float azScanDeg = 60.0f;
    /** @brief Elevation bars scanned (1/2/4). */
    int scanBars = 4;
    /** @brief High/medium PRF selection; true means high PRF. */
    bool highPrf = true;
    /** @brief Acquisition cursor position in UI space, each axis in [-1, 1]. */
    MfdVec2 cursorPosition {};
    /** @brief Antenna elevation command in degrees, [-30, 30]. */
    float antennaElevationDeg = 0.0f;
    /** @brief Index of the bugged track, or -1 when no track is designated. */
    int buggedTrack = -1;
    /** @brief Animated antenna azimuth in degrees, updated by the simulation. */
    float antennaAzimuthDeg = 0.0f;
};

/**
 * @brief Stores-management state used by the SMS page.
 */
struct StoresState
{
    /** @brief Selected weapon station, 1-based; 0 means none. */
    int selectedStation = 3;
    /** @brief True when a physical store is still carried on each 1-based station. */
    std::array<bool, kStationCount> stationLoaded {
        {true, true, true, true, true, true, true, true, true}};
    /** @brief Number of ready rounds for the selected weapon. */
    int readyCount = 4;
    /** @brief Ripple quantity for A-G profiles. */
    int rippleQuantity = 1;
    /** @brief Ripple pairs flag for A-G profiles. */
    int ripplePairs = 1;
    /** @brief Release/impact timer in seconds for the selected profile. */
    int releaseSeconds = 20;
    /** @brief Active delivery profile number. */
    int profile = 1;
    /** @brief Selected A-G delivery mode for the bomb profile page. */
    AirGroundDeliveryMode deliveryMode = AirGroundDeliveryMode::Ccrp;
    /** @brief Release interval between ripple weapons in seconds. */
    float releaseIntervalSeconds = 0.50f;
    /** @brief True when the jet is in SIM (simulated) release. */
    bool simulateMode = true;
    /** @brief True when MASTER ARM is set to ARM. */
    bool masterArm = false;
    /** @brief True while the demo release timer is counting down. */
    bool releaseInProgress = false;
    /** @brief Remaining impact countdown for the latest simulated release. */
    float impactTimeRemainingSeconds = 0.0f;
    /** @brief Station used by the latest simulated release. */
    int releasedStation = 0;
};

/**
 * @brief Returns whether a 1-based station carries the demo A-G weapon family.
 * @param station 1-based station number.
 * @return `true` for stations used by the A-G release page.
 */
inline bool IsAirGroundStoreStation(const int station) noexcept
{
    return station == 3 || station == 6 || station == 7;
}

/**
 * @brief Counts stations that still carry a store.
 * @param stores Stores-management state to inspect.
 * @return Number of loaded stations in [0, kStationCount].
 */
inline int CountLoadedStores(const StoresState& stores) noexcept
{
    int count = 0;
    for (const bool loaded : stores.stationLoaded)
    {
        if (loaded)
        {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Returns whether one 1-based station currently carries a store.
 * @param stores Stores-management state to inspect.
 * @param station 1-based station number.
 * @return `true` when `station` is valid and loaded.
 */
inline bool IsStationLoaded(const StoresState& stores, const int station) noexcept
{
    if (station < 1 || station > static_cast<int>(kStationCount))
    {
        return false;
    }
    return stores.stationLoaded[static_cast<std::size_t>(station - 1)];
}

/**
 * @brief Counts loaded A-G release stations.
 * @param stores Stores-management state to inspect.
 * @return Number of loaded A-G stations.
 */
inline int CountReadyAirGroundStores(const StoresState& stores) noexcept
{
    int count = 0;
    for (int station = 1; station <= static_cast<int>(kStationCount); ++station)
    {
        if (IsAirGroundStoreStation(station) && IsStationLoaded(stores, station))
        {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Clears every store from the SMS inventory, as after a full jettison.
 * @param stores Stores-management state to mutate.
 */
inline void JettisonAllStores(StoresState& stores) noexcept
{
    stores.stationLoaded.fill(false);
    stores.selectedStation = 0;
    stores.readyCount = 0;
    stores.rippleQuantity = 0;
    stores.ripplePairs = 0;
    stores.releaseSeconds = 0;
    stores.profile = 0;
    stores.masterArm = false;
    stores.releaseInProgress = false;
    stores.impactTimeRemainingSeconds = 0.0f;
    stores.releasedStation = 0;
}

/**
 * @brief One HSD steerpoint entry in polar form relative to ownship.
 */
struct Steerpoint
{
    /** @brief Steerpoint number label. */
    int number = 1;
    /** @brief Bearing from ownship to the steerpoint in degrees true. */
    float bearingDeg = 0.0f;
    /** @brief Range from ownship to the steerpoint in nautical miles. */
    float rangeNm = 0.0f;
};

/**
 * @brief Navigation state used by the HSD page.
 */
struct NavState
{
    /** @brief Currently selected steerpoint number. */
    int currentSteerpoint = 4;
    /** @brief Number of active steerpoints in the authored flight plan. */
    int waypointCount = 6;
    /** @brief HSD range scale in nautical miles (radius of the compass rose). */
    float rangeScaleNm = 80.0f;
    /** @brief True when the HSD display is centered on ownship. */
    bool centeredDisplay = true;
    /** @brief True when secondary HSD symbols are hidden. */
    bool declutterActive = false;
    /** @brief Bearing from ownship to bullseye in degrees true. */
    float bullseyeBearingDeg = 250.0f;
    /** @brief Range from ownship to bullseye in nautical miles. */
    float bullseyeRangeNm = 42.0f;
    /** @brief Flight-plan steerpoints. */
    std::array<Steerpoint, kSteerpointCount> steerpoints {};
};

/**
 * @brief Complete semantic sample consumed by the MFD projection and controller.
 *
 * Fill this once per rendered frame. It is treated as a full snapshot: previous
 * values are not remembered by the stateless projection.
 */
struct MfdInputSample
{
    /** @brief Aircraft master mode shared by the pages. */
    MasterMode masterMode = MasterMode::AirToAir;
    /** @brief Page that currently owns the option-select buttons. */
    MfdPage activePage = MfdPage::Radar;
    /** @brief Ownship attitude and performance state. */
    OwnshipState ownship {};
    /** @brief Operator radar controls. */
    RadarSettings radar {};
    /** @brief Sensor-native fire-control-radar tracks. */
    std::array<RadarTrack, kMaxRadarTracks> tracks {};
    /** @brief Stores-management state. */
    StoresState stores {};
    /** @brief Navigation state. */
    NavState nav {};
};

/** @brief Projected B-scope presentation of one radar track. */
struct RadarTrackView
{
    /** @brief Direct projection of @ref RadarTrack::active. */
    bool visible = false;
    /** @brief Direct projection of the radar-owned track lifecycle state. */
    RadarTrackState state = RadarTrackState::Search;
    /** @brief B-scope position in MFD space. */
    MfdVec2 position {};
    /** @brief Track heading rotation in degrees relative to the display. */
    float rotationDegrees = 0.0f;
    /** @brief Derived target altitude in thousands of feet MSL. */
    float altitudeThousandsFt = 0.0f;
    /** @brief True when the track is hostile. */
    bool hostile = true;
    /** @brief True when the displayed position is extrapolated. */
    bool extrapolated = false;
};

/**
 * @brief Projected FCR (Radar) page geometry.
 */
struct RadarFrame
{
    /** @brief Per-track projected B-scope symbology. */
    std::array<RadarTrackView, kMaxRadarTracks> tracks {};
    /** @brief True when live radar symbology should be rendered. */
    bool radarPresentationVisible = true;
    /** @brief True when the azimuth sweep line should be drawn (hidden in STT). */
    bool scanLineVisible = true;
    /** @brief Azimuth sweep line horizontal position in MFD space. */
    float scanLineX = 0.0f;
    /** @brief Antenna elevation caret vertical position in MFD space. */
    float elevationCaretY = 0.0f;
    /** @brief Acquisition cursor position in MFD space. */
    MfdVec2 cursorPosition {};
    /** @brief Maximum searched altitude at cursor range, in thousands of feet MSL. */
    float cursorMaximumAltitudeThousandsFt = 0.0f;
    /** @brief Minimum searched altitude at cursor range, in thousands of feet MSL. */
    float cursorMinimumAltitudeThousandsFt = 0.0f;
    /** @brief True when a bugged or STT target should be drawn. */
    bool buggedVisible = false;
    /** @brief Slot of the radar-published system target or STT track, or -1. */
    int designatedTrackIndex = -1;
    /** @brief Projected presentation of the bugged track. */
    RadarTrackView buggedTrack {};
    /** @brief True when the target data block should be drawn. */
    bool datablockVisible = false;
    /** @brief True when the STT collision-antenna-train-angle steering cross is valid. */
    bool sttInterceptVisible = false;
    /** @brief STT steering-cross position; its vertical coordinate matches target range. */
    MfdVec2 sttInterceptPosition {};
};

/** @brief Projected HSD symbol position (steerpoint or bullseye). */
struct NavSymbolView
{
    /** @brief True when the symbol should be drawn this frame. */
    bool visible = false;
    /** @brief Symbol position in MFD space. */
    MfdVec2 position {};
};

/** @brief Projected HSD line segment between two flight-plan steerpoints. */
struct FlightPlanLegView
{
    /** @brief True when both endpoints are visible on the HSD range scale. */
    bool visible = false;
    /** @brief Center of the line segment in MFD space. */
    MfdVec2 position {};
    /** @brief Segment rotation in degrees. */
    float rotationDegrees = 0.0f;
    /** @brief Segment length in MFD logical units. */
    float length = 0.0f;
};

/**
 * @brief Projected HSD (NAV) page geometry.
 */
struct NavFrame
{
    /** @brief Compass-rose rotation in degrees so ownship heading points up. */
    float compassRotationDegrees = 0.0f;
    /** @brief Per-steerpoint projected position. */
    std::array<NavSymbolView, kSteerpointCount> steerpoints {};
    /** @brief Visible HSD route legs connecting consecutive steerpoints. */
    std::array<FlightPlanLegView, kSteerpointCount - 1U> flightPlanLegs {};
    /** @brief Projected bullseye symbol. */
    NavSymbolView bullseye {};
};

/**
 * @brief Complete projected frame for all three MFD pages.
 */
struct MfdFrame
{
    /** @brief FCR page projection. */
    RadarFrame radar {};
    /** @brief HSD page projection. */
    NavFrame nav {};
};

/** @} */
} // namespace lhld
