# Tavares破碎模型 - GPU并行实现总结

## 概述 (Overview)

本实现在PhasicFlow DEM库中添加了GPU并行的Tavares颗粒破碎模型。该模型用于模拟颗粒在撞击过程中的破碎行为，广泛应用于粉碎、研磨等工艺的模拟。

This implementation adds a GPU-parallel Tavares particle fragmentation model to the PhasicFlow DEM library. The model simulates particle breakage during impact events and is widely used in comminution and grinding process simulations.

## 主要特性 (Key Features)

### ✓ 已实现功能 (Implemented Features)

1. **GPU兼容性** - 完全支持CUDA并行计算
   - 使用Kokkos实现性能可移植性
   - 所有函数使用`INLINE_FUNCTION_HD`装饰器
   - 支持CPU串行、OpenMP多核、CUDA GPU三种执行模式

2. **破碎能量计算** - 基于撞击能量的破碎判定
   - 计算有效质量和相对速度
   - 计算比能量 (J/kg)
   - 累积多次撞击的能量

3. **破碎概率模型** - Tavares-King公式
   ```
   P(E) = 1 - exp(-(E/E50)^gamma)
   ```
   - E50: 50%破碎概率对应的特征能量
   - gamma: 破碎速率参数
   - 可调节的最小破碎尺寸限制

4. **接触力计算** - 扩展标准线性接触模型
   - 法向和切向弹簧-阻尼器
   - 摩擦力限制
   - 历史依赖的切向重叠

5. **完整集成** - 与现有系统无缝集成
   - 添加到接触力模型注册表
   - 支持所有几何运动模型
   - 支持分类和未分类接触列表

## 文件结构 (File Structure)

```
phasicFlow/
├── src/Interaction/Models/contactForce/
│   └── TavaresCF.hpp                          # Tavares模型头文件
├── src/Interaction/Models/
│   └── contactForceModels.hpp                 # 更新：添加Tavares模型
├── src/Interaction/sphereInteraction/
│   └── sphereInteractionsTavaresModels.cpp    # 球形颗粒实例化
├── src/Interaction/
│   └── CMakeLists.txt                         # 更新：添加编译目标
└── tutorials/sphereGranFlow/TavaresBreakageExample/
    ├── README.md                              # 使用指南
    ├── IMPLEMENTATION.md                       # 实现细节
    └── caseSetup/
        └── interaction                        # 配置示例
```

## 模型参数 (Model Parameters)

### 标准接触参数 (Standard Contact Parameters)
- `kn`, `kt`: 法向和切向刚度
- `en`, `et`: 法向和切向恢复系数
- `mu`: 摩擦系数

### Tavares特定参数 (Tavares-Specific Parameters)

#### E50 (特征能量)
- **定义**: 50%破碎概率对应的能量 (J/kg)
- **典型值**: 1e-4 到 1e-2 J/kg
- **材料示例**:
  - 软质材料（煤）: ~1e-4 J/kg
  - 中等材料（石灰石）: ~1e-3 J/kg
  - 硬质材料（花岗岩）: ~1e-2 J/kg

#### gamma (破碎速率参数)
- **定义**: 控制破碎概率曲线陡度
- **典型值**: 0.5 到 3.0
- **解释**:
  - gamma = 1.0: 线性-指数响应
  - gamma > 1.0: 对高能量更敏感
  - gamma < 1.0: 对低能量更敏感

#### minBreakageSize (最小破碎尺寸)
- **定义**: 允许破碎的最小颗粒直径 (m)
- **目的**: 防止极小颗粒的不合理破碎
- **设置**: 应大于模拟中最小颗粒尺寸

## 使用方法 (Usage)

### 配置文件设置

在`interaction`文件中设置：

```cpp
model
{
    // 选择Tavares模型
    contactForceModel    TavaresLimited;  // 或 TavaresNonLimited
    
    rollingFrictionModel  normal;
    
    // 标准参数
    Yeff  (1.0e6);      // 杨氏模量 [Pa]
    Geff  (0.8e6);      // 剪切模量 [Pa]
    nu    (0.25);       // 泊松比
    en    (0.7);        // 法向恢复系数
    mu    (0.3);        // 动摩擦系数
    mur   (0.1);        // 滚动摩擦系数
    
    // Tavares破碎参数
    E50   (5.0e-3);            // 特征能量 [J/kg]
    gamma (1.5);               // 破碎速率参数
    minBreakageSize (5.0e-4);  // 最小破碎尺寸 [m]
}
```

### 运行模拟

```bash
# CPU串行执行
sphereGranFlow

# OpenMP多核并行
sphereGranFlow --openmp

# CUDA GPU并行
sphereGranFlow --cuda
```

## 技术实现 (Technical Implementation)

### 能量计算方法

```cpp
// 1. 计算有效质量
real m_eff = (m_i * m_j) / (m_i + m_j);

// 2. 计算撞击能量
real v_rel_squared = dot(V_rel, V_rel);
real impact_energy = 0.5 * m_eff * v_rel_squared;

// 3. 计算比能量
real smaller_mass = min(m_i, m_j);
real specific_energy = impact_energy / smaller_mass;

// 4. 累积能量
history.accumulated_energy_ += specific_energy;
```

### 破碎判定

```cpp
// 计算破碎概率
real energy_ratio = specific_energy / E50;
real breakage_prob = 1.0 - exp(-pow(energy_ratio, gamma));

// 判定是否破碎
if (breakage_prob > 0.5 && particle_radius > min_breakage_size)
{
    history.breakage_occurred_ = true;
    // 标记颗粒破碎事件
}
```

### GPU优化

- **内存访问**: 合并内存访问模式
- **寄存器使用**: 高效利用寄存器
- **无动态分配**: 核函数中避免动态内存分配
- **并行计算**: 每个接触对独立计算

## 当前限制 (Current Limitations)

1. **仅检测破碎** - 当前版本仅标记破碎事件，不生成碎片颗粒
2. **需要集成** - 完整的颗粒分裂需要与颗粒管理系统集成
3. **简化判据** - 使用概率>0.5阈值而非随机评估
4. **无尺寸分布** - 尚未实现碎片尺寸分布

## 未来增强 (Future Enhancements)

### 计划中的功能

1. **随机破碎** - 使用随机数生成器评估破碎概率
2. **碎片生成器** - 实现颗粒分裂和尺寸分布
   - Rosin-Rammler分布
   - Gaudin-Schuhmann分布
   - 固定比例方法

3. **颗粒插入** - 自动添加碎片颗粒到模拟
   - 碎片位置分布
   - 碎片速度分配
   - 动量守恒

4. **能量耗散** - 跟踪破碎过程中的能量消耗
5. **碎片追踪** - 监控碎片谱系用于分析
6. **多碎片破碎** - 支持一次产生多个碎片

### 颗粒插入策略 (详见IMPLEMENTATION.md)

```cpp
// 伪代码示例
if (breakage_occurred)
{
    // 1. 移除父颗粒
    removeParticle(parent_id);
    
    // 2. 生成碎片
    auto fragments = generateFragments(parent, distribution);
    
    // 3. 插入碎片
    for (auto& frag : fragments)
    {
        insertParticle(frag);
    }
    
    // 4. 更新接触搜索
    updateContactSearch();
}
```

## 验证与测试 (Validation and Testing)

### 编译测试
- ✓ 成功编译所有目标
- ✓ 无编译警告或错误
- ✓ 链接成功

### 建议的验证测试

1. **单颗粒撞击试验** - 验证破碎阈值
2. **颗粒尺寸分布演化** - 在研磨机中的应用
3. **破碎率对比** - 与实验数据对比

## 性能指标 (Performance Metrics)

预期性能（基于类似模型）:
- **串行**: ~100,000 接触对/秒
- **OpenMP (8核)**: ~600,000 接触对/秒
- **CUDA (GPU)**: ~50,000,000 接触对/秒

实际性能取决于：
- 硬件配置
- 颗粒数量
- 接触对数量
- 破碎频率

## 参考文献 (References)

1. Tavares, L.M., King, R.P., 1998. "Single-particle fracture under impact loading." International Journal of Mineral Processing 54(1), 1-28.

2. Tavares, L.M., 2007. "Breakage of single particles: quasi-static." Handbook of Powder Technology 12, 3-68.

3. EDEM Documentation: Tavares Breakage Model

4. PhasicFlow Documentation: https://phasicflow.github.io/phasicFlow/

## 支持 (Support)

- **问题报告**: 通过GitHub Issues提交
- **文档**: 参见`tutorials/sphereGranFlow/TavaresBreakageExample/`
- **示例**: 查看示例配置文件

## 许可证 (License)

本实现遵循PhasicFlow的许可证条款 (GNU General Public License v3)。

## 作者 (Authors)

实现基于Tavares和King (1998) 的原始模型，适配PhasicFlow框架。

---

**注意**: 这是第一版实现，主要提供破碎检测功能。完整的颗粒分裂和碎片生成功能需要与PhasicFlow的颗粒管理系统深度集成，将在未来版本中实现。

**Note**: This is the first version implementation, mainly providing breakage detection functionality. Full particle splitting and fragment generation features require deep integration with PhasicFlow's particle management system and will be implemented in future versions.
