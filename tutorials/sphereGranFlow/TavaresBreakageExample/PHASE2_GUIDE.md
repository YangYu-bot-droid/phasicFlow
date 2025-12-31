# Tavares Breakage Model - Phase 2 Implementation Guide

## Overview

Phase 2 adds infrastructure for particle fragmentation with breakage event collection and fragment generation capabilities. This enables the Tavares model to not just detect breakage, but to provide all information needed to replace broken particles with daughter particles.

## What's New in Phase 2

### 1. Breakage Event System

**TavaresBreakageEvent** data structure captures all information about a breakage event:

```cpp
struct TavaresBreakageEvent
{
    uint32  particleId;           // ID of particle that broke
    real    diameter;             // Diameter of parent particle
    real    impactEnergy;         // Impact energy at breakage
    realx3  position;             // Position of breakage
    realx3  velocity;             // Velocity at breakage
    uint32  propertyId;           // Material property ID
    real    breakageProbability;  // Calculated breakage probability
};
```

### 2. Fragment Generation Parameters

**TavaresFragmentParams** defines how to generate daughter particles:

```cpp
struct TavaresFragmentParams
{
    uint32  numFragments;         // Number of daughter particles
    real    sizeRatio;            // Size ratio for fragments
    real    energyFraction;       // Fraction of kinetic energy retained
    real    rr_x0;                // Rosin-Rammler characteristic size
    real    rr_n;                 // Rosin-Rammler uniformity parameter
};
```

### 3. Fragment Size Distribution

The system includes methods for generating fragment sizes with mass conservation:

```cpp
// Calculate fragment size using distribution
real fragmentSize(real parentDiameter, uint32 fragmentIndex) const;

// Normalize fragment sizes to conserve mass
real normalizedFragmentSize(real parentDiameter, uint32 fragmentIndex) const;

// Check total volume
real totalFragmentVolume(real parentDiameter) const;
```

## Configuration Parameters

### New Phase 2 Parameters

Add these to your interaction configuration file:

```cpp
model
{
    contactForceModel    TavaresLimited;
    
    // ... standard parameters ...
    
    // Phase 2: Fragment generation parameters
    numFragments (2);         // Number of daughter particles (2-10)
    sizeRatio (0.7);          // Fragment size ratio (0.5-0.9)
    energyFraction (0.5);     // Energy retention (0.3-0.7)
}
```

#### Parameter Descriptions:

**numFragments** (integer)
- **Definition**: Number of daughter particles created when parent breaks
- **Typical values**: 2-10
- **Material guidance**:
  - Brittle materials (glass, ceramics): 5-10 (multiple fragments)
  - Semi-brittle (rocks, ores): 3-5 (moderate fragmentation)
  - Ductile materials (some minerals): 2-3 (binary or ternary split)
- **Computational cost**: Higher values increase particle count

**sizeRatio** (real, dimensionless)
- **Definition**: Controls relative size of fragment particles
- **Range**: 0.5 to 0.9
- **Interpretation**:
  - 0.5: Highly unequal fragments (one large, others small)
  - 0.7: Moderate size distribution (recommended default)
  - 0.9: Nearly equal-sized fragments
- **Effect**: Combined with numFragments determines size distribution

**energyFraction** (real, dimensionless)
- **Definition**: Fraction of parent kinetic energy retained by fragments
- **Range**: 0.3 to 0.7
- **Interpretation**:
  - Remaining energy (1 - energyFraction) is dissipated in:
    - Fracture surface creation
    - Heat generation
    - Sound/vibration
  - Lower values: More energy dissipation (typical for brittle materials)
  - Higher values: More energy retention (higher fragment velocities)

## Fragment Size Distribution Methods

### Current Implementation

The default implementation uses a power-law distribution:

```cpp
real baseFraction = pow(sizeRatio, 1.0 / numFragments);
real sizeFactor = pow(baseFraction, fragmentIndex + 1);
real d_fragment = parentDiameter * sizeFactor;
```

### Mass Conservation

All fragment sizes are automatically normalized to conserve mass:

```cpp
// Parent volume
V_parent = (4/3) * π * (d_parent/2)³

// Total fragment volume must equal parent volume
Σ V_fragment_i = V_parent

// Normalization factor
scale = (V_parent / Σ V_fragment)^(1/3)

// Final fragment sizes
d_fragment_normalized = d_fragment * scale
```

### Alternative Distributions (for future enhancement)

**Rosin-Rammler Distribution**
```
F(x) = 1 - exp(-(x/x₀)ⁿ)
```
- x₀: Characteristic size parameter
- n: Uniformity parameter (shape of distribution)
- Widely used in comminution engineering

**Gaudin-Schuhmann Distribution**
```
F(x) = (x/x_max)^m
```
- x_max: Maximum fragment size
- m: Distribution modulus

## Usage Example

### Step 1: Configure Model

```cpp
// In interaction file
model
{
    contactForceModel    TavaresLimited;
    
    // Standard contact parameters
    Yeff  (1.0e6);
    en    (0.7);
    mu    (0.3);
    
    // Tavares breakage parameters
    E50   (5.0e-3);           // 5 mJ/kg characteristic energy
    gamma (1.5);              // Breakage rate parameter
    minBreakageSize (5.0e-4); // 0.5 mm minimum size
    
    // Phase 2: Fragment parameters
    numFragments (3);         // Ternary breakage
    sizeRatio (0.65);         // Moderate size variation
    energyFraction (0.45);    // 45% energy retained, 55% dissipated
}
```

### Step 2: Access Breakage Events (in simulation code)

```cpp
// During contact force calculations, the model flags breakage
// After force calculation loop, collect events:

if (history.breakage_occurred_)
{
    // Get fragment parameters
    auto fragParams = model.getFragmentParams(propId_i, propId_j, Ri, Rj);
    
    // Create breakage event
    auto event = model.createBreakageEvent(
        particleId,
        diameter,
        impactEnergy,
        position,
        velocity,
        propertyId,
        breakageProb
    );
    
    // Store event for processing
    breakageEvents.push_back(event);
}
```

### Step 3: Process Breakage Events (between time steps)

```cpp
// For each breakage event
for (const auto& event : breakageEvents)
{
    // 1. Get fragment parameters
    TavaresFragmentParams params = getFragmentParams(...);
    
    // 2. Generate fragment positions
    realx3Vector fragmentPositions(params.numFragments);
    for (uint32 i = 0; i < params.numFragments; ++i)
    {
        // Add small perturbation to avoid overlaps
        fragmentPositions[i] = event.position + randomPerturbation();
    }
    
    // 3. Generate fragment sizes (mass-conserving)
    realVector fragmentDiameters(params.numFragments);
    for (uint32 i = 0; i < params.numFragments; ++i)
    {
        fragmentDiameters[i] = params.normalizedFragmentSize(
            event.diameter, i
        );
    }
    
    // 4. Calculate fragment velocities
    realx3Vector fragmentVelocities(params.numFragments);
    real totalFragmentMass = 0.0;
    for (uint32 i = 0; i < params.numFragments; ++i)
    {
        real fragMass = ρ * (4/3) * π * pow(fragmentDiameters[i]/2, 3);
        totalFragmentMass += fragMass;
    }
    
    for (uint32 i = 0; i < params.numFragments; ++i)
    {
        // Distribute velocity with random scatter
        realx3 scatter = randomUnitVector() * scatterMagnitude;
        fragmentVelocities[i] = event.velocity * sqrt(params.energyFraction) + scatter;
    }
    
    // 5. Remove parent particle
    particles.markForRemoval(event.particleId);
    
    // 6. Insert daughter particles
    wordVector shapeNames(params.numFragments, shapeName);
    particles.insertParticles(fragmentPositions, shapeNames, propertyList);
}

// 7. Update contact search
contactSearch.updateStructure();
```

## Integration Architecture

### Contact Force Kernel (GPU)
- Calculates forces and breakage probability
- Flags breakage events in contact history
- **Does NOT** modify particle list directly

### Event Collection (CPU/GPU)
- Scan contact histories for breakage flags
- Extract breakage event data
- Store in event buffer

### Event Processing (CPU)
- Process events between time steps
- Perform particle insertion/removal
- Update spatial data structures
- Maintain contact lists

## Performance Considerations

### Memory Usage

Each breakage event requires:
- ~64 bytes for event data
- Additional memory for fragment generation

Expected overhead:
- 1000 breakage events: ~64 KB
- 10000 breakage events: ~640 KB

### Computational Cost

- **Fragment generation**: O(numFragments) per event
- **Particle insertion**: O(numFragments * log(N)) per event
- **Contact search update**: O(N log N) once per batch

### GPU Compatibility

The breakage event system is designed for GPU execution:
- Event detection in GPU kernels
- Event data copied to host for processing
- Particle insertion performed on host
- Updated particle data transferred back to device

## Validation

### Mass Conservation Check

```cpp
real parentMass = ρ * (4/3) * π * pow(d_parent/2, 3);
real totalFragmentMass = 0.0;

for (uint32 i = 0; i < numFragments; ++i)
{
    real d_frag = params.normalizedFragmentSize(d_parent, i);
    real m_frag = ρ * (4/3) * π * pow(d_frag/2, 3);
    totalFragmentMass += m_frag;
}

assert(abs(totalFragmentMass - parentMass) / parentMass < 1e-6);
```

### Energy Balance

```cpp
real parentKE = 0.5 * parentMass * dot(v_parent, v_parent);
real totalFragmentKE = 0.0;

for (const auto& frag : fragments)
{
    totalFragmentKE += 0.5 * frag.mass * dot(frag.velocity, frag.velocity);
}

real dissipatedEnergy = parentKE - totalFragmentKE;
real expectedDissipation = parentKE * (1 - energyFraction);

assert(abs(dissipatedEnergy - expectedDissipation) / parentKE < 0.01);
```

## Example Material Parameters

### Granite (Hard, Brittle)
```cpp
E50   (1.2e-2);         // High energy threshold
gamma (2.0);            // Sharp breakage threshold
numFragments (6);       // Multiple fragments
sizeRatio (0.6);        // Varied fragment sizes
energyFraction (0.35);  // High energy dissipation
```

### Limestone (Medium)
```cpp
E50   (5.0e-3);         // Medium energy threshold
gamma (1.5);            // Moderate sensitivity
numFragments (4);       // Moderate fragmentation
sizeRatio (0.7);        // Moderate size variation
energyFraction (0.45);  // Moderate energy retention
```

### Coal (Soft, Friable)
```cpp
E50   (8.0e-4);         // Low energy threshold
gamma (1.0);            // Gradual breakage
numFragments (3);       // Limited fragments
sizeRatio (0.75);       // Less size variation
energyFraction (0.55);  // Lower energy dissipation
```

## Future Enhancements

1. **Stochastic Breakage**: Use random number generator for probabilistic breakage
2. **Advanced Distributions**: Implement Rosin-Rammler, Gaudin-Schuhmann
3. **Abrasion Model**: Add surface wear alongside bulk breakage
4. **Fragment Orientation**: Track and assign angular velocities
5. **Damage Evolution**: Track cumulative damage beyond simple energy accumulation

## References

1. Tavares, L.M., King, R.P., 1998. "Single-particle fracture under impact loading." Int. J. Miner. Process. 54, 1-28.

2. Tavares, L.M., 2007. "Breakage of single particles: quasi-static." Handbook of Powder Technology 12, 3-68.

3. Vogel, L., Peukert, W., 2003. "Breakage behaviour of different materials - construction of a master curve for the breakage probability." Powder Technology 129, 101-110.

4. Altair EDEM 2022 Documentation: Tavares UFRJ Breakage Model

## Contact

For questions or issues with Phase 2 implementation, refer to the main documentation or create an issue in the repository.
