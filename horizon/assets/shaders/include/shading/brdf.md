看到各路大佬都手推过pbr的公式，写过各种shading model，自己只在准备校招的时候照着rtr4推了一下diffuse和specular的brdf，也没太深究进去，在pbr的流程上一直一知半解，写代码的时候只会抄learnopengl和ue的shader，但很多都绕不开pbs（ssao，ibl啥的），所以开一个blog记录一下学习的过程，顺便给自己的renderer定义一个材质标准。

这篇blog将会包括

pbr defination

cook torrance model
disney model
ue
filament

pbs cource

multi scatter
multi bounce

image based lighting


## microfacet brdf


cook torrance specular brdf

$f_s = \frac{F(h, l)· G(l, v, h) ·D(h)}{4 · |n · l||n·v|}$

### fresnel

菲涅尔相关之前写过一个notes，总体来说菲涅尔方程反映了入射光打到表面时会入射角度不同，反射和折射的比例也不同

### ndf

微观世界中，一个物体的表面法线不一，我们定义微表面法线方向为m，只有m=h的微表面才会将光线反射到视线中，所以brdf中用的h就是微表面表面的法线，我们用一个函数NDF来描述不同粗糙度的情况下，m=h的比例

![half](https://pic3.zhimg.com/80/v2-ed0f40ccb3ecfa18342b5667e6c55312_720w.webp)


一般用GGX

### mask shadow function

崎岖不平的微表面会产生遮挡，遮挡函数G1(w, w_h)表示微表面法线为h时，从v方向观察能看到表面的比例w

用入射和出射都会有概率被遮挡，用$G_2 = G_1(l,m)G_1(v,m)$表示

maskshadowfunction可以消掉下面的NoV和NoL

learnopengl用的是schlick
ue用的是smithggx

## subsurface scatter brdf

lambert diffuse
$L_o = \int_{\Omega} f_r · L_i · \cos\theta·d\omega_i$

$L_o = \int_{\Omega} f_{diff} · L_i · \cos\theta·d\omega_i + \int_{\Omega} f_{spec} · L_i · \cos\theta·d\omega_i$

diffuse在各个方向上的brdf都是一个常数

$\rho_{ss} = \int_{\Omega} f_{diff} · \cos\theta·d\omega_i$

$\rho_{ss} = \int_{\Omega} f_{diff} · \cos\theta·d\omega_i$

转成球面积分

$\rho_{ss} = f_{diff} ·\int_{0}^{2\pi}\int_{0}^{\frac{\pi}{2}}{\cos\theta·\sin\theta· d\theta d\phi}$

$f_{diff}  = \frac{\rho_{ss}}{\pi}$


### smooth subsurface

smooth subsurface的特性：subsurface的irregular和scattering length相比很小，所以这种subsurface不受粗糙度影响，那么diffuse的部分就等于 1 - pow(1-nol, 5)，outgoing light也有refraction，要再乘 1-pow(1-nov, 5)

### rough subsurface

rough subsurface的特性：scattering length 和subsurface的irregular和相比很小，粗糙度会影响diffuse的强度，主要是逆反射，表面越粗糙，逆反射越强，在grazing angle处最为明显。

disney将这两种情况混合起来计算一个diffuse，次表面散射使用Hanrahan-Krueger brdf，用一个subsurface参数混合这两者


## disney 


# ref

https://www.one-tab.com/page/6WWtv26xQxaOvDl-Ywy7FQ
https://zhuanlan.zhihu.com/p/95824400
https://zhuanlan.zhihu.com/p/61962884
https://ubm-twvideo01.s3.amazonaws.com/o1/vault/gdc2017/Presentations/Hammon_Earl_PBR_Diffuse_Lighting.pdf
