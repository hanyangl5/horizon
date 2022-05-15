
## transmittance LUT

任意一点$p$到任意点$q$的透射率T可由下式得到

$T=exp(-\sum_{i\in{Rayleigh,Mie}}\int_{p}^{q}\beta_i^{e}(y)dy)$

我们可以通过计算沿$pq$方向，$p$到大气顶层$i$的透射率，$q$到大气顶层$i$的透射率得到$p$到$q$的透射率

$T = \frac{exp(-\sum_{i\in{Rayleigh,Mie}}\int_{i}^{q}\beta_i^{e}(y)dy)}{exp(-\sum_{i\in{Rayleigh,Mie}}\int_{i}^{p}\beta_i^{e}(y)dy)}$

![](https://img-blog.csdnimg.cn/20190106185316143.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMxNjE1OTE5,size_16,color_FFFFFF,t_70)

我们可以将任意一点$p$到大气顶层$i$的透射率记为$T(r,\mu)$，其中$r=||x||$，$\mu$是$p$到$i$方向与天顶的夹角余弦，用一张256x64的2Dtexture存储$T(r,\mu)$，256表示$\mu$，64表示$r$

计算$r，\mu$，直接给出原文的代码，

```
void GetRMuFromTransmittanceTextureUv(AtmosphereParameters params, vec2 uv, out float r, out float mu) {
    float x_mu = GetUnitRangeFromTextureCoord(uv.x, TRANSMITTANCE_TEXTURE_WIDTH);
    float x_r = GetUnitRangeFromTextureCoord(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT);
    // Distance to top atmosphere boundary for a horizontal ray at ground level.
    float H = sqrt(params.top_radius * params.top_radius -
        params.bottom_radius * params.bottom_radius);
    // Distance to the horizon, from which we can compute r:
    float rho = H * x_r;
    r = sqrt(rho * rho + params.bottom_radius * params.bottom_radius);
    // Distance to the top atmosphere boundary for the ray (r,mu), and its minimum
    // and maximum values over all mu - obtained for (r,1) and (r,mu_horizon) -
    // from which we can recover mu:
    float d_min = params.top_radius - r;
    float d_max = rho + H;
    float d = d_min + x_mu * (d_max - d_min);
    mu = d == 0.0 ? float(1.0) : (H * H - rho * rho - d * d) / (2.0 * r * d);
    mu = clamp(mu, -1.0, 1.0);
}
```

首先计算$x_r$,$x_\mu$，将[0, 1)的uv映射到[0.5/size,1-0.5/size]上

首先是$r$，通过勾股定理计算出沿地平线方向的距离$H$，乘上比例$x\_r$，再用勾股定理计算出$r$
![](r.png)

我们记$pq$长度为$d$，任意点q的坐标可以由$d，r，\mu$表示
$(d\sqrt{1-\mu^2},r+d\mu)$

计算mu时，首先计算了$d\_min$和$d\_max$两个值，分别代表如下两种情况
![](mu.png)，通过$x_\mu$在中间插值得到一个中间值$d$。

我们考虑点i，由勾股定理

$r_i^2=r_{i_{x}}^2+r_{i_{y}}^2$

$r_{top}^2=(d\sqrt{1-\mu^2})^2 + (r+d\mu)^2$

$r_{top}^2=d^2+2r\mu d+r^2$

其中$r_{top}^2-r^2 = H^2-\rho^2(H^2 = r_{top}^2-r_{bottom}^2, \rho^2 = r^2 - r_{bottom}^2)$

得到$H^2 -\rho^2 -d^2=2rd\mu$

$\mu = \frac{H^2-\rho^2-d^2}{2rd}$



> 为什么用这么复杂的方式计算,原文解释为直接用$cos$计算$\mu$会有artifact

得到$r，\mu$接下来计算从p到大气顶层的透射率

代码
```
float DistanceToTopAtmosphereBoundary(AtmosphereParameters params, float r, float mu) {

    float discriminant = r * r * (mu * mu - 1.0) + params.top_radius * params.top_radius;
    return max(-r * mu + max(discriminant, 0.0), 0.0);
}

float GetLayerDensity(DensityLayer layer, float altitude) {
    float density = layer.exp_term * exp(layer.exp_scale * altitude) +
        layer.linear_term * altitude + layer.constant_term;
    return clamp(density, float(0.0), float(1.0));
}

float GetProfileDensity(Density profile, float altitude) {
    return altitude < profile.layers[0].width ? GetLayerDensity(profile.layers[0], altitude) : GetLayerDensity(profile.layers[1], altitude);
}

float ComputeOpticalLengthToTopAtmosphereBoundary(AtmosphereParameters params, Density profile, float r, float mu) {
  // float of intervals for the numerical integration.
    const int SAMPLE_COUNT = 500;
  // The integration step, i.e. the length of each integration interval.
    float dx = DistanceToTopAtmosphereBoundary(params, r, mu) / float(SAMPLE_COUNT);
  // Integration loop.
    float result = 0.0f;
    for(int i = 0; i <= SAMPLE_COUNT; ++i) {
        float d_i = float(i) * dx;
    // Distance between the current sample point and the planet center.
        float r_i = sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r);
    // float density at the current sample point (divided by the number density
    // at the bottom of the atmosphere, yielding a dimensionless number).
        float y_i = GetProfileDensity(profile, r_i - params.bottom_radius);
    // Sample weight (from the trapezoidal rule).
        float weight_i = i == 0 || i == SAMPLE_COUNT ? 0.5 : 1.0;
        result += y_i * weight_i * dx;
    }
    return result;
}

vec3 ComputeTransmittanceToTopAtmosphereBoundary(AtmosphereParameters params, float r, float mu) {
    return exp(-(params.rayleigh_scattering *
        ComputeOpticalLengthToTopAtmosphereBoundary(params, params.rayleigh_density, r, mu) +
        params.mie_extinction *
        ComputeOpticalLengthToTopAtmosphereBoundary(params, params.mie_density, r, mu) +
        params.absorption_extinction *
        ComputeOpticalLengthToTopAtmosphereBoundary(params, params.absorption_density, r, mu)));
}
```

$T(r,\mu)$的计算的方法：

OpticalLength，即光从大气顶层$i$到$(r,\mu)$的衰减，$\int_{(r,\mu)}^i density(x)dx$

对三种粒子分别计算高度$0$处的透射率，乘上$(r,\mu)$处的OpticalLength为最终的透射率$T(r,\mu)$，得到transmittance

## single scattering LUT

主要函数如下

从r，mu得到uv

```
vec2 GetTransmittanceTextureUvFromRMu(AtmosphereParameters params,
    float r, float mu) {
  // Distance to top params boundary for a horizontal ray at ground level.
  float H = sqrt(params.top_radius * params.top_radius -
      params.bottom_radius * params.bottom_radius);
  // Distance to the horizon.
  float rho =
      sqrt(max(r * r - params.bottom_radius * params.bottom_radius, 0.0));
  // Distance to the top params boundary for the ray (r,mu), and its minimum
  // and maximum values over all mu - obtained for (r,1) and (r,mu_horizon).
  float d = DistanceToTopAtmosphereBoundary(params, r, mu);
  float d_min = params.top_radius - r;
  float d_max = rho + H;
  float x_mu = (d - d_min) / (d_max - d_min);
  float x_r = rho / H;
  return vec2(GetTextureCoordFromUnitRange(x_mu, TRANSMITTANCE_TEXTURE_WIDTH),
              GetTextureCoordFromUnitRange(x_r, TRANSMITTANCE_TEXTURE_HEIGHT));
}
```

通过r，mu得到某一点处的到大气顶层的透射率

```
vec3 GetTransmittanceToTopAtmosphereBoundary(
    AtmosphereParameters params,
    sampler2D transmittance_texture,
    float r, float mu) {
  vec2 uv = GetTransmittanceTextureUvFromRMu(params, r, mu);
  return vec3(texture(transmittance_texture, uv));
}
```

通过r，mu，d计算出r_q, mu_q
$r_{d} = \sqrt{d^2+2r\mu d+r^2}$
$\mu_{d} = (r\mu+d)/r_d$

采样2次得到p到大气顶层的透射率，q到大气顶层的透射率，计算出pq的透射率

假设pq不与地面相交：pq的透射率$T(r,\mu)/T(r_d,\mu_d)$
相交：$T(r,-\mu)/T(r_d,-\mu_d)$


```

vec3 GetTransmittance(
    AtmosphereParameters params,
    sampler2D transmittance_texture,
    float r, float mu, float d, bool ray_r_mu_intersects_ground) {

  float r_d = clamp(sqrt(d * d + 2.0 * r * mu * d + r * r), params.bottom_radius, params.top_radius);
  float mu_d = clamp((r * mu + d) / r_d, -1.0, 1.0);

  if (ray_r_mu_intersects_ground) {
    return min(
        GetTransmittanceToTopAtmosphereBoundary(
            params, transmittance_texture, r_d, -mu_d) /
        GetTransmittanceToTopAtmosphereBoundary(
            params, transmittance_texture, r, -mu),
        vec3(1.0));
  } else {
    return min(
        GetTransmittanceToTopAtmosphereBoundary(
            params, transmittance_texture, r, mu) /
        GetTransmittanceToTopAtmosphereBoundary(
            params, transmittance_texture, r_d, mu_d),
        vec3(1.0));
  }
}

```

计算点q沿mu_s到大气顶层的透射率$T_{LQ}$，吗mu_s是的天顶角的余弦，nu是op和pq夹角余弦，

$nu = , mu_s = $

![](fig)

```
vec3 GetTransmittanceToSun(AtmosphereParameters params, sampler2D transmittance_texture, float r, float mu_s) {
    float sin_theta_h = params.bottom_radius / r;
    float cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));
    return GetTransmittanceToTopAtmosphereBoundary(params, transmittance_texture, r, mu_s) *
        smoothstep(-sin_theta_h * params.sun_angular_radius / rad, sin_theta_h * params.sun_angular_radius / rad, mu_s - cos_theta_h);
}

```

计算单次散射的被积函数，其组成为$\sum_{i\in rayleigh,mie}\ (T_{LQ}T_{QP} )\ density_i\ L_{sun}\ P(i)$ 我们将会将单次散射预计算到lut中，因此把光照强度L_{sun}和相位函数P(i)移出积分，最终计算的时候再加进来

```
void ComputeSingleScatteringIntegrand(AtmosphereParameters params, sampler2D transmittance_texture, float r, float mu, float mu_s, float nu, float d, bool ray_r_mu_intersects_ground, out vec3 rayleigh, out vec3 mie) {
    float r_d = clamp(sqrt(d * d + 2.0 * r * mu * d + r * r), params.bottom_radius, params.top_radius);
    float mu_s_d = clamp((r * mu_s + d * nu) / r_d, -1.0f, 1.0f);
    vec3 transmittance = GetTransmittance(params, transmittance_texture, r, mu, d, ray_r_mu_intersects_ground) *
        GetTransmittanceToSun(params, transmittance_texture, r_d, mu_s_d);

    rayleigh = transmittance * GetProfileDensity(params.rayleigh_density, r_d - params.bottom_radius);
    mie = transmittance * GetProfileDensity(params.mie_density, r_d - params.bottom_radius);
}
```

计算单次散射

```

void ComputeSingleScattering(AtmosphereParameters params, sampler2D transmittance_texture, float r, float mu, float mu_s, float nu, bool ray_r_mu_intersects_ground, out vec3 rayleigh, out vec3 mie) {

    // float of intervals for the numerical integration.
    const int SAMPLE_COUNT = 50;
    // The integration step, i.e. the float of each integration interval.
    float dx = DistanceToNearestAtmosphereBoundary(params, r, mu, ray_r_mu_intersects_ground) / float(SAMPLE_COUNT);
    // Integration loop.
    vec3 rayleigh_sum = vec3(0.0);
    vec3 mie_sum = vec3(0.0);
    for(int i = 0; i <= SAMPLE_COUNT; ++i) {
        float d_i = float(i) * dx;
        // The Rayleigh and Mie single scattering at the current sample point.
        vec3 rayleigh_i;
        vec3 mie_i;
        ComputeSingleScatteringIntegrand(params, transmittance_texture, r, mu, mu_s, nu, d_i, ray_r_mu_intersects_ground, rayleigh_i, mie_i);
        // Sample weight (from the trapezoidal rule).
        float weight_i = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;
        rayleigh_sum += rayleigh_i * weight_i;
        mie_sum += mie_i * weight_i;
    }
    rayleigh = rayleigh_sum * dx * params.solar_irradiance *
        params.rayleigh_scattering;
    mie = mie_sum * dx * params.solar_irradiance * params.mie_scattering;
}
```

预计算single scattering LUT，单次散射的预计算定义一个4Dtexture，将参数$(r,\mu,\mu_s,\nu)$映射到$(u,v,w,z)$上，即可查询出单次散射的值

```
void ComputeSingleScatteringTexture(AtmosphereParameters params, sampler2D transmittance_texture, in vec3 frag_coord, out vec3 rayleigh, out vec3 mie) {
    float r;
    float mu;
    float mu_s;
    float nu;
    bool ray_r_mu_intersects_ground;
    GetRMuMuSNuFromScatteringTextureFragCoord(params, frag_coord, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
    ComputeSingleScattering(params, transmittance_texture, r, mu, mu_s, nu, ray_r_mu_intersects_ground, rayleigh, mie);
}
```
