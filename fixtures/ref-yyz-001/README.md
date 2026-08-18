# REF-YYZ-001 R0 qualification bundle

This directory provides the canonical executable source and asset selection
for the current R0 YYZ qualification scope. `source.json` keeps two profiles
separate: the executable two-interval Cartesian qualification mission and the
30 second target architecture example from 00A. The latter remains
`target_pending` and contributes no passing scientific fields.

The qualification profile starts from position `[0, 0, 1000] m`, velocity
`[110, 0, 0] m/s`, identity `q_I_B`, zero body rate and `100 kg` committed
mass. Its vehicle subject is `vehicle.fixture.yyz@1`, active at initialize.
It executes two `0.1 s` lookup-composed FrozenIntervals. The current committed
boundary drives altitude guidance, pitch-moment control and the unit-gain ideal
moment transform. Three committed samples support the terminal decision,
terminal observation and result.

`asset-index.json` selects twelve executable component fixture/oracle/model
identities and seven concrete input families: uniform environment, mass and
inertia, the trilinear aerodynamic table, propulsion, guidance/control,
numerical policy and terminal observation. The canonical verifier confirms
every value against the source cases used by the mission composition.

The independent 80-digit Python mission reference and the C++17 probe are
compared leaf by leaf. `oracles/ref-yyz-001/reference.json` records one exact
or numeric tolerance entry for every observed probe leaf, including the
allowed bound and absolute difference. It also retains the existing fourteen
input rejections, ten directed mutations and three structured precommit
diagnostics, plus six canonical source/asset boundary rejections.

After configuring and building a preset, run CTest test
`r0.yyz-canonical-bundle.oracle`.
