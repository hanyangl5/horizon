horizon render pipeline language

how to declare a pass

pass BasePass : compute
{
    setup()
    {

    }

    resources
    {
        material_tex : tex2darray
        taa_offsets : buf
        gbuffer0 : rt0
        gbuffer1 : rt1
        gbuffer2 : rt2
        gbuffer3 : rt3
        depth_rt : depth_stencil
    }

}

pass ScreenSpaceReflection : compute
{
    setup()
    {

    }

    resources
    {
        depth_tex : tex2d
        normal_tex :tex2d
        color_tex : tex2d
        view_buf : buf
        trace_result :rwtex
    }

}





## Builtin Pass

copy pass
downsample pass(downsample method)
generatemipmap pass