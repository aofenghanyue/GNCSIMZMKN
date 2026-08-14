# YYZ supplied uniform environment executable oracle

`REF-YYZ-UNIFORM-ENVIRONMENT-001` fixes the accepted fixture-local pure
query for supplied environment fields. Every valid finite query returns the
same physical response:

```text
gravity_I(query) = supplied gravity_I
airmass_velocity_I(query) = supplied airmass_velocity_I
density(query) = supplied density
speed_of_sound(query) = supplied speed_of_sound
```

The query carries an inertial Cartesian position, sample tick, clock domain
and configuration revision. The response carries those identities, the
uniform model identity, `Valid` quality and the four supplied fields. Gravity
and air-mass velocity are inertial-frame free vectors.

The executable also consumes the response through the already accepted
air-data and rigid-core relations:

```text
v_relative_I = v_vehicle_I - airmass_velocity_I
qbar = 0.5 * density * dot(v_relative_I, v_relative_I)
Mach = norm(v_relative_I) / speed_of_sound
acceleration_I = force_I / mass + gravity_I
```

The consumer link inherits the accepted air-data requirement `V > 0` and
requires a finite positive mass. These consumer restrictions do not narrow
the valid domain of the uniform environment response itself.

One case joins ordinary supplied constants to both consumers. A second case
proves that zero density and a positive speed of sound below one metre per
second remain valid without clamps. Position/tick invariance is checked at a
high positive z coordinate. Legacy-style altitude density and gravity decay
are explicit rejected mutations.

The Python implementation uses only standard-library `decimal`. The C++17
probe independently implements the query, consumer links and failure paths.
Neither implementation imports product or Legacy code.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.yyz-uniform-environment" --output-on-failure
```

Earth shape and rotation, geodetic conversion, altitude-dependent atmosphere
or gravity, pressure, temperature, humidity, wind profiles, gusts, canonical
constants, assets and product contracts remain outside this fixture-local
slice.
