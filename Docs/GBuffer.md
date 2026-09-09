# GBuffer Layout

## Render targets

| Target | Format | Channel layout |
| --- | --- | --- |
| MRT0 | `R10G10B10A2` | `RGB`: Emissive, `A`: Unused |
| MRT1 | `R10G10B10A2` | `RG`: Oct-encoded normal, `BA`: Packed roughness and specular |
| MRT2 | `RGBA8` | `RGB`: Base color, `A`: Metallic |
| MRT3 | `RGBA8` | `RGB`: Encoded motion vector (MV), `A`: Material ID |

## Packed material properties

MRT1 provides 12 bits across its `B10A2` channels. They are packed as one logical value:

| Logical bits | Contents |
| --- | --- |
| `[7:0]` | 8-bit roughness |
| `[11:8]` | 4-bit specular |

The lower 10 bits are stored in `B`; the upper 2 bits are stored in `A`.

```hlsl
uint packed = uint(round(saturate(roughness) * 255.0));
packed |= uint(round(saturate(specular) * 15.0)) << 8;

gbuffer1.b = float(packed & 0x3ff) / 1023.0;
gbuffer1.a = float(packed >> 10) / 3.0;
```

```hlsl
uint packed = uint(round(gbuffer1.b * 1023.0));
packed |= uint(round(gbuffer1.a * 3.0)) << 10;

float roughness = float(packed & 0xff) / 255.0;
float specular = float((packed >> 8) & 0xf) / 15.0;
```

The packed channels must be read without filtering. The 8-bit material ID supports up to 256 values.

## Motion vectors

MRT3 packs each motion-vector component into 12 bits across its `RGB8` channels. Motion is stored in normalized screen UV space over the range `[-1, 1]`; `A8` stores the material ID.

## Depth and stencil

| Attachment | Format | Contents |
| --- | --- | --- |
| Depth | `D24S8` | 24-bit depth and 8-bit stencil |

`Oct` refers to octahedral normal encoding. `MV` refers to the encoded motion vector.

## Future work

- Implement OpenPBR material evaluation.
