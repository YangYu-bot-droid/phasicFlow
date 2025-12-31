# Tavares Breakage Model Example

This example demonstrates the use of the GPU-parallel Tavares particle fragmentation model in PhasicFlow.

## Overview

The Tavares and King (1998) breakage model is a widely-used particle fragmentation model in DEM simulations of comminution processes such as:
- Crushing and grinding
- Impact mills
- Drop weight testers
- Rock fragmentation

## Model Description

The model calculates the probability of particle breakage based on impact energy:

```
P(E) = 1 - exp(-(E/E50)^gamma)
```

Where:
- `E` = Specific impact energy (J/kg)
- `E50` = Characteristic energy for 50% breakage probability
- `gamma` = Breakage rate parameter

## GPU Compatibility

This implementation is fully compatible with GPU execution:
- Uses Kokkos for performance portability
- All functions decorated with `INLINE_FUNCTION_HD` for host/device execution
- Parallel execution on CUDA-enabled GPUs
- Seamless integration with existing PhasicFlow GPU infrastructure

## Configuration Parameters

### Standard Contact Force Parameters
- `kn`, `kt`: Normal and tangential stiffness
- `en`, `et`: Normal and tangential restitution coefficients
- `mu`: Friction coefficient

### Tavares-Specific Parameters

#### E50 (Characteristic Energy)
- **Definition**: Energy required for 50% breakage probability (J/kg)
- **Typical values**: 1e-4 to 1e-2 J/kg
- **Material examples**:
  - Soft materials (coal): ~1e-4 J/kg
  - Medium materials (limestone): ~1e-3 J/kg
  - Hard materials (granite): ~1e-2 J/kg
- **Effect**: Lower values make particles easier to break

#### gamma (Breakage Rate Parameter)
- **Definition**: Controls the steepness of the breakage probability curve
- **Typical values**: 0.5 to 3.0
- **Interpretation**:
  - gamma = 1.0: Linear-exponential response
  - gamma > 1.0: More sensitive to high impact energies
  - gamma < 1.0: More sensitive to low impact energies

#### minBreakageSize (Minimum Breakage Size)
- **Definition**: Minimum particle diameter for breakage (m)
- **Purpose**: Prevents unrealistic fragmentation of very small particles
- **Setting**: Should be larger than the smallest particle in your simulation

## Usage

1. **Select the Model**: In the interaction file, set:
   ```
   contactForceModel    TavaresLimited;
   ```
   or
   ```
   contactForceModel    TavaresNonLimited;
   ```

2. **Configure Parameters**: Set the Tavares-specific parameters:
   ```
   E50   (5.0e-3);           // Characteristic energy
   gamma (1.5);              // Breakage rate parameter
   minBreakageSize (5.0e-4); // Minimum size for breakage
   ```

3. **Run Simulation**: Execute with CPU (OpenMP) or GPU (CUDA):
   ```bash
   # CPU execution
   sphereGranFlow --openmp
   
   # GPU execution
   sphereGranFlow --cuda
   ```

## Model Variants

### TavaresLimited
- Applies friction limiting with tangential overlap correction
- Recommended for most applications

### TavaresNonLimited
- Standard friction limiting without overlap correction
- May be faster for some scenarios

## Current Implementation Status

This implementation provides:
✓ Breakage probability calculation
✓ Impact energy tracking
✓ Contact force computation
✓ GPU compatibility
✓ Integration with existing contact models

Future enhancements (requires particle management integration):
- Automatic particle splitting upon breakage
- Fragment size distribution (Rosin-Rammler, etc.)
- Daughter particle generation
- Mass conservation during fragmentation

## References

1. Tavares, L.M., King, R.P., 1998. "Single-particle fracture under impact loading." International Journal of Mineral Processing 54(1), 1-28.

2. Tavares, L.M., 2007. "Breakage of single particles: quasi-static." Handbook of Powder Technology 12, 3-68.

## Contact Force Model Structure

The Tavares model extends the standard linear contact force model with:
- Energy accumulation tracking
- Breakage probability calculation
- Breakage event flagging in contact history

## See Also

- Main PhasicFlow documentation: https://phasicflow.github.io/phasicFlow/
- Tutorial examples: ../
- Contact force models: src/Interaction/Models/contactForce/
