#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
//
// Buffer Definitions: 
//
// Resource bind info for instances
// {
//
//   struct MeshInstance
//   {
//       
//       struct Transform
//       {
//           
//           float3 translate;          // Offset:    0
//           float scale;               // Offset:   12
//           float4 rotation;           // Offset:   16
//
//       } transform;                   // Offset:    0
//       float radius;                  // Offset:   32
//       
//       struct PairUInt16
//       {
//           
//           uint val;                  // Offset:   36
//
//       } mesh_index_and_max_distance; // Offset:   36
//
//   } $Element;                        // Offset:    0 Size:    40
//
// }
//
// Resource bind info for temp_1
// {
//
//   uint $Element;                     // Offset:    0 Size:     4
//
// }
//
// Resource bind info for temp_1_counter
// {
//
//   uint $Element;                     // Offset:    0 Size:     4
//
// }
//
// Resource bind info for out_temp_2
// {
//
//   uint $Element;                     // Offset:    0 Size:     4
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      ID      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- ------- -------------- ------
// instances                         texture  struct         r/o      T0             t3      1 
// temp_1                            texture  struct         r/o      T1             t4      1 
// temp_1_counter                    texture  struct         r/o      T2             t5      1 
// out_temp_2                            UAV  struct     r/w+cnt      U0             u2      1 
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// no Input
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// no Output
cs_5_1
dcl_globalFlags refactoringAllowed
dcl_resource_structured T0[3:3], 40, space=0
dcl_resource_structured T1[4:4], 4, space=0
dcl_resource_structured T2[5:5], 4, space=0
dcl_uav_structured_opc U0[2:2], 4, space=0
dcl_input vThreadID.x
dcl_temps 2
dcl_thread_group 64, 1, 1
ld_structured r0.x, l(0), l(0), T2[5].xxxx
uge r0.x, vThreadID.x, r0.x
if_nz r0.x
  ret 
endif 
ld_structured r0.x, vThreadID.x, l(0), T1[4].xxxx
ld_structured r0.y, r0.x, l(32), T0[3].xxxx
lt r0.y, l(0.000000), r0.y
if_nz r0.y
  imm_atomic_alloc r1.x, U0[2]
  store_structured U0[2].x, r1.x, l(0), r0.x
endif 
ret 
// Approximately 13 instruction slots used
#endif

const BYTE g_filter_inst_frustum_cs[] =
{
     68,  88,  66,  67, 238, 236, 
    245, 205,  20, 221, 154, 176, 
    182,  45, 146, 230,  48,   7, 
    108, 212,   1,   0,   0,   0, 
    188,   6,   0,   0,   5,   0, 
      0,   0,  52,   0,   0,   0, 
     76,   4,   0,   0,  92,   4, 
      0,   0, 108,   4,   0,   0, 
     32,   6,   0,   0,  82,  68, 
     69,  70,  16,   4,   0,   0, 
      4,   0,   0,   0,   8,   1, 
      0,   0,   4,   0,   0,   0, 
     60,   0,   0,   0,   1,   5, 
     83,  67,   0,   5,   0,   0, 
    232,   3,   0,   0,  19,  19, 
     68,  37,  60,   0,   0,   0, 
     24,   0,   0,   0,  40,   0, 
      0,   0,  40,   0,   0,   0, 
     36,   0,   0,   0,  12,   0, 
      0,   0,   0,   0,   0,   0, 
    220,   0,   0,   0,   5,   0, 
      0,   0,   6,   0,   0,   0, 
      1,   0,   0,   0,  40,   0, 
      0,   0,   3,   0,   0,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 230,   0, 
      0,   0,   5,   0,   0,   0, 
      6,   0,   0,   0,   1,   0, 
      0,   0,   4,   0,   0,   0, 
      4,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0, 237,   0,   0,   0, 
      5,   0,   0,   0,   6,   0, 
      0,   0,   1,   0,   0,   0, 
      4,   0,   0,   0,   5,   0, 
      0,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   2,   0,   0,   0, 
    252,   0,   0,   0,  11,   0, 
      0,   0,   6,   0,   0,   0, 
      1,   0,   0,   0,   4,   0, 
      0,   0,   2,   0,   0,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 105, 110, 
    115, 116,  97, 110,  99, 101, 
    115,   0, 116, 101, 109, 112, 
     95,  49,   0, 116, 101, 109, 
    112,  95,  49,  95,  99, 111, 
    117, 110, 116, 101, 114,   0, 
    111, 117, 116,  95, 116, 101, 
    109, 112,  95,  50,   0, 171, 
    220,   0,   0,   0,   1,   0, 
      0,   0, 104,   1,   0,   0, 
     40,   0,   0,   0,   0,   0, 
      0,   0,   3,   0,   0,   0, 
    230,   0,   0,   0,   1,   0, 
      0,   0, 112,   3,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,   3,   0,   0,   0, 
    237,   0,   0,   0,   1,   0, 
      0,   0, 152,   3,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,   3,   0,   0,   0, 
    252,   0,   0,   0,   1,   0, 
      0,   0, 192,   3,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,   3,   0,   0,   0, 
    144,   1,   0,   0,   0,   0, 
      0,   0,  40,   0,   0,   0, 
      2,   0,   0,   0,  76,   3, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0,  36,  69, 
    108, 101, 109, 101, 110, 116, 
      0,  77, 101, 115, 104,  73, 
    110, 115, 116,  97, 110,  99, 
    101,   0, 116, 114,  97, 110, 
    115, 102, 111, 114, 109,   0, 
     84, 114,  97, 110, 115, 102, 
    111, 114, 109,   0, 116, 114, 
     97, 110, 115, 108,  97, 116, 
    101,   0, 102, 108, 111,  97, 
    116,  51,   0, 171,   1,   0, 
      3,   0,   1,   0,   3,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    196,   1,   0,   0, 115,  99, 
     97, 108, 101,   0, 102, 108, 
    111,  97, 116,   0,   0,   0, 
      3,   0,   1,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    246,   1,   0,   0, 114, 111, 
    116,  97, 116, 105, 111, 110, 
      0, 102, 108, 111,  97, 116, 
     52,   0,   1,   0,   3,   0, 
      1,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  41,   2, 
      0,   0, 186,   1,   0,   0, 
    204,   1,   0,   0,   0,   0, 
      0,   0, 240,   1,   0,   0, 
    252,   1,   0,   0,  12,   0, 
      0,   0,  32,   2,   0,   0, 
     48,   2,   0,   0,  16,   0, 
      0,   0,   5,   0,   0,   0, 
      1,   0,   8,   0,   0,   0, 
      3,   0,  84,   2,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 176,   1, 
      0,   0, 114,  97, 100, 105, 
    117, 115,   0, 109, 101, 115, 
    104,  95, 105, 110, 100, 101, 
    120,  95,  97, 110, 100,  95, 
    109,  97, 120,  95, 100, 105, 
    115, 116,  97, 110,  99, 101, 
      0,  80,  97, 105, 114,  85, 
     73, 110, 116,  49,  54,   0, 
    118,  97, 108,   0, 100, 119, 
    111, 114, 100,   0,   0,   0, 
     19,   0,   1,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    206,   2,   0,   0, 202,   2, 
      0,   0, 212,   2,   0,   0, 
      0,   0,   0,   0,   5,   0, 
      0,   0,   1,   0,   1,   0, 
      0,   0,   1,   0, 248,   2, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    191,   2,   0,   0, 166,   1, 
      0,   0, 120,   2,   0,   0, 
      0,   0,   0,   0, 156,   2, 
      0,   0, 252,   1,   0,   0, 
     32,   0,   0,   0, 163,   2, 
      0,   0,   4,   3,   0,   0, 
     36,   0,   0,   0,   5,   0, 
      0,   0,   1,   0,  10,   0, 
      0,   0,   3,   0,  40,   3, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    153,   1,   0,   0, 144,   1, 
      0,   0,   0,   0,   0,   0, 
      4,   0,   0,   0,   2,   0, 
      0,   0, 212,   2,   0,   0, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 144,   1,   0,   0, 
      0,   0,   0,   0,   4,   0, 
      0,   0,   2,   0,   0,   0, 
    212,   2,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    144,   1,   0,   0,   0,   0, 
      0,   0,   4,   0,   0,   0, 
      2,   0,   0,   0, 212,   2, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0,  77, 105, 
     99, 114, 111, 115, 111, 102, 
    116,  32,  40,  82,  41,  32, 
     72,  76,  83,  76,  32,  83, 
    104,  97, 100, 101, 114,  32, 
     67, 111, 109, 112, 105, 108, 
    101, 114,  32,  49,  48,  46, 
     49,   0,  73,  83,  71,  78, 
      8,   0,   0,   0,   0,   0, 
      0,   0,   8,   0,   0,   0, 
     79,  83,  71,  78,   8,   0, 
      0,   0,   0,   0,   0,   0, 
      8,   0,   0,   0,  83,  72, 
     69,  88, 172,   1,   0,   0, 
     81,   0,   5,   0, 107,   0, 
      0,   0, 106,   8,   0,   1, 
    162,   0,   0,   7,  70, 126, 
     48,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   3,   0, 
      0,   0,  40,   0,   0,   0, 
      0,   0,   0,   0, 162,   0, 
      0,   7,  70, 126,  48,   0, 
      1,   0,   0,   0,   4,   0, 
      0,   0,   4,   0,   0,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0, 162,   0,   0,   7, 
     70, 126,  48,   0,   2,   0, 
      0,   0,   5,   0,   0,   0, 
      5,   0,   0,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
    158,   0, 128,   7,  70, 238, 
     49,   0,   0,   0,   0,   0, 
      2,   0,   0,   0,   2,   0, 
      0,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,  95,   0, 
      0,   2,  18,   0,   2,   0, 
    104,   0,   0,   2,   2,   0, 
      0,   0, 155,   0,   0,   4, 
     64,   0,   0,   0,   1,   0, 
      0,   0,   1,   0,   0,   0, 
    167,   0,   0,  10,  18,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0,   0,   0,   6, 112, 
     32,   0,   2,   0,   0,   0, 
      5,   0,   0,   0,  80,   0, 
      0,   6,  18,   0,  16,   0, 
      0,   0,   0,   0,  10,   0, 
      2,   0,  10,   0,  16,   0, 
      0,   0,   0,   0,  31,   0, 
      4,   3,  10,   0,  16,   0, 
      0,   0,   0,   0,  62,   0, 
      0,   1,  21,   0,   0,   1, 
    167,   0,   0,   9,  18,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,   2,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
      6, 112,  32,   0,   1,   0, 
      0,   0,   4,   0,   0,   0, 
    167,   0,   0,  10,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
     32,   0,   0,   0,   6, 112, 
     32,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,  49,   0, 
      0,   7,  34,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
     26,   0,  16,   0,   0,   0, 
      0,   0,  31,   0,   4,   3, 
     26,   0,  16,   0,   0,   0, 
      0,   0, 178,   0,   0,   6, 
     18,   0,  16,   0,   1,   0, 
      0,   0,   0, 224,  33,   0, 
      0,   0,   0,   0,   2,   0, 
      0,   0, 168,   0,   0,  10, 
     18, 224,  33,   0,   0,   0, 
      0,   0,   2,   0,   0,   0, 
     10,   0,  16,   0,   1,   0, 
      0,   0,   1,  64,   0,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   0,   0,   0,   0, 
     21,   0,   0,   1,  62,   0, 
      0,   1,  83,  84,  65,  84, 
    148,   0,   0,   0,  13,   0, 
      0,   0,   2,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   2,   0,   0,   0, 
      2,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   3,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      1,   0,   0,   0,   1,   0, 
      0,   0
};