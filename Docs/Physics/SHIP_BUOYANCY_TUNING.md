# Physical ocean response

## Scope and diagnosis

The saved Quang Ninh level already uses real Water buoyancy. There is no need to animate a visual mesh, add an orientation controller, freeze rotation, replace navigation, or alter Cesium collision. The measured baseline was overly resistant to angular motion: all profiles used roll resistance 30/s and pitch resistance 15/s, in addition to existing body damping and fitted inertia.

Only the profile roll/pitch resistance rates are tuned. Mass, center of mass, inertia, pontoons, displacement, vertical resistance, sideways resistance, speed limits, propulsion, rudder and the ocean asset are preserved. The existing local angular drag uses physical torque against angular velocity. It has no desired angle, sine wave, rotation assignment or velocity replacement, and does not damp yaw.

The source is `/Game/LandscapeMaterials/Ocean/Custom_Waves_Ocean`: GerstnerWaterWaves / GerstnerWaterWaveGeneratorSimple, 10 waves, seed 0, wavelengths 3000–8000 cm, component amplitudes 35–70 cm, wind angle -90°, directional spread 30°. It is unchanged. Individual sample-point wave heights need not equal the spatially averaged heave of a hull supported by multiple pontoons.

## Repeatable protocol

`Tools/Testing/measure_wave_response.py` starts the saved map through the actual manual setup menu. It stops the fixture fleet's decisions and creates one transient physical player probe at a documented valid water location. Only its initial placement is assigned. Each Small/Medium/Large probe settles for 20 simulation seconds, then measures idle 45s, cruise 20s, sustained turn 20s, reverse 20s and reverse-fire 20s. The original Enhanced Input actions and weapon dispatcher supply propulsion, rudder and projectiles. Nothing writes a measured hull's pose or velocity after initialization.

Measurements use simulation time at approximately 15 Hz. Raw telemetry includes attitude, position, vertical velocity, actual wave surface height/normal, angular velocity, forward speed, water contact, physical simulation, shoreline guard and shot counters. Separate shoreline and patrol regressions cover legitimate island proximity and traffic.

The original engine `GetWaterSurfaceInfoAtLocation` helper omits `IncludeWaves`. An editor-only `SampleOceanWaves` probe explicitly requests the physical wave surface. Initial baseline surface telemetry is therefore excluded from wave-height comparisons; its hull attitude and velocity measurements remain valid. No production water sampling policy was changed.

RMS and P95 in the final table are **absolute hull angles**, including mean trim: RMS = sqrt(mean(angle²)), P95 = percentile95(abs(angle)). Centered motion RMS/P95 are retained separately, alongside mean trim and absolute maxima. This avoids disguising a persistent tilt by subtracting it from the reported absolute P95. Angles are never clamped to achieve the target.

The first angular-drag trial produced Small/Medium/Large centered idle pitch P95 of 0.97/1.67/0.71°. Its Medium absolute pitch P95 was 2.56°, so Medium pitch resistance was raised from the trial's 3/s to 5/s before its full-protocol probe spawned. All trials and raw data remain in Saved; final measurements and compact evidence follow after validation.

## Configuration and limits

Edit `DA_SmallShip`, `DA_MediumShip`, or `DA_LargeShip` under `/Game/NavalBattle/Profiles`, then Buoyancy → Stability → Roll/Pitch Damping Per Second. Higher rates resist motion more strongly. These fields are independent of the Movement speed/propulsion/rudder settings. Persistent assets are saved individually through Unreal; the map and ocean are not resaved as part of this tuning.

Small retains mass 90 kg, inertia dimension scale (1,1,20), body angular damping 0.65. Medium retains 150 kg, (1,1,6), damping 0.5. Large retains 240 kg, (1,1,6), damping 0.65 and its six-support layout. These are the project's established gameplay masses and fitted collision-body dimensions, not real vessel displacement claims. Actual tensor depends on the collision body; the scale values alone do not compare effective inertia across different hulls. Horizontal dimension scales remain 1 to preserve yaw response.

Heave resistance remains 20/s and lateral resistance 4/s. A 1–2° target is approximate calm-water behavior, not an angle limit or guarantee at every wave phase or heading. Directional waves, mean trim, contact with legitimate terrain, steering and firing can produce different excursions. Finite-duration PIE checks do not certify arbitrary frame rates, extreme sea states, multiplayer or packaged builds.

## Initial verified tuning and measurements (Small superseded below)

| Hull | Roll rate before/after (1/s) | Pitch rate before/after (1/s) |
|---|---|---|
| Small | 30 / 3 | 15 / 2 |
| Medium | 30 / 4 | 15 / 5 |
| Large | 30 / 6 | 15 / 4 |

All 15 hull/state checks passed. Every measured hull remained physical, alive and in water for all samples. Each hull passed forward speed, sustained-turn yaw, negative reverse speed and ten original weapon shots while reversing. The largest observed pitch excursion across the complete tuned protocol was 2.31 degrees (Large reverse); this is reported rather than clamped.

| Hull | State | Roll RMS before/after | Pitch RMS before/after | Roll P95 before/after | Pitch P95 before/after |
|---|---|---|---|---|---|
| Small | idle | 0.009/0.034 | 0.150/0.572 | 0.018/0.065 | 0.268/1.004 |
| Small | cruise | 0.008/0.021 | 0.053/0.154 | 0.012/0.034 | 0.106/0.284 |
| Small | turn | 0.023/0.074 | 0.059/0.149 | 0.033/0.113 | 0.123/0.319 |
| Small | reverse | 0.034/0.134 | 0.202/0.725 | 0.061/0.231 | 0.415/1.508 |
| Small | reverse_fire | 0.040/0.174 | 0.199/0.703 | 0.064/0.251 | 0.394/1.371 |
| Medium | idle | 0.046/0.136 | 0.973/1.058 | 0.079/0.231 | 1.296/1.651 |
| Medium | cruise | 0.051/0.090 | 1.008/1.051 | 0.062/0.131 | 1.159/1.383 |
| Medium | turn | 0.296/0.360 | 0.940/0.956 | 0.509/0.600 | 1.078/1.224 |
| Medium | reverse | 0.602/0.532 | 1.104/1.177 | 0.674/0.750 | 1.310/1.616 |
| Medium | reverse_fire | 0.530/0.311 | 0.941/1.012 | 0.590/0.458 | 1.201/1.395 |
| Large | idle | 0.084/0.173 | 0.997/1.160 | 0.107/0.223 | 1.269/1.799 |
| Large | cruise | 0.074/0.139 | 0.926/0.940 | 0.095/0.202 | 1.057/1.195 |
| Large | turn | 0.055/0.042 | 0.978/1.014 | 0.102/0.073 | 1.186/1.463 |
| Large | reverse | 0.180/0.226 | 1.094/1.397 | 0.220/0.316 | 1.478/2.282 |
| Large | reverse_fire | 0.245/0.286 | 1.038/1.162 | 0.278/0.366 | 1.380/1.998 |

![Raw measured idle hull attitude](../Testing/images/buoyancy-idle.png)

Complete numeric data, centered statistics, contact, speed and angular-velocity statistics: [BUOYANCY_METRICS.json](../Testing/BUOYANCY_METRICS.json). The full protocol used the compiled physical implementation and final profile parameters. A subsequent native rebuild synchronized the fallback Medium pitch value; the integrated regression verifies that rebuilt executable. Raw recordings remain in Saved/WAVE_BASELINE.json and Saved/WAVE_TUNED_FULL.json.

## Small follow-up requested on 2026-11-07

The user requested approximately 2–3 degree movement for Small. Its final roll/pitch resistance is now **1.5/s and 0.8/s**, superseding the initial 3/s and 2/s values above. Medium remains 4/s and 5/s; Large remains 6/s and 4/s. All other hull, wave and movement parameters are unchanged. The saved Data Asset and native missing-asset fallback use the same rates.

The complete five-state physical trial passed, with 100% water-contact samples. Small idle absolute pitch P95/max was 2.03/2.18 degrees; reverse P95/max 2.51/2.67 degrees; reverse-fire P95/max 2.37/2.52 degrees with ten original shots. Cruise and turn excursions were smaller because response depends on wave phase, heading and encounter speed. This is a physical response target, not a constant imposed angle or a 3-degree clamp.

[Latest Small metrics](../Testing/SMALL_WAVE_FOLLOWUP.json) supersede the Small rows in the initial comparison. The original three-hull evidence is retained as history.

| Small state | Roll RMS | Pitch RMS | Roll P95 | Pitch P95 |
|---|---|---|---|---|
| idle | 0.069 | 1.117 | 0.125 | 2.034 |
| cruise | 0.053 | 0.706 | 0.075 | 0.970 |
| turn | 0.354 | 0.569 | 0.597 | 0.969 |
| reverse | 0.635 | 1.205 | 0.737 | 2.507 |
| reverse_fire | 0.653 | 1.179 | 0.793 | 2.374 |

The rebuilt final Small profile also passed real player forward travel, both rudder directions, mouse aim, firing and camera release. At the shoreline it retained 100% water contact, reached a maximum 2.64-degree pitch and physically reversed 99.6 metres. The [raw Small trace](../Testing/Traces/small-wave-final.csv) records simulation seconds, phase, angles in degrees, vertical/forward speeds in m/s, wave-relative heave and contact/shot flags.
