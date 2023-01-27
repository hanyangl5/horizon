#define UPDATE_FREQ_NONE space0
#define UPDATE_FREQ_PER_FRAME space1
#define UPDATE_FREQ_PER_BATCH space2
#define UPDATE_FREQ_PER_DRAW space3
#define UPDATE_FREQ_BINDLESS space4

// #define CBUFFER(NAME, FREQ, REG, BINDING) [[vk::binding(BINDING, FREQ)]] cbuffer NAME : register(REG, FREQ)
// #define RES(TYPE, NAME, FREQ, REG, BINDING) [[vk::binding(BINDING, FREQ)]] TYPE NAME : register(REG, FREQ)

#define CBUFFER(NAME, FREQ) cbuffer NAME : register(FREQ)
#define RES(TYPE, NAME, FREQ) TYPE NAME : register(FREQ)

