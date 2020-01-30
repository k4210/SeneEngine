#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
//
// Buffer Definitions: 
//
// cbuffer SceneManagerParams
// {
//
//   uint instances_num;                // Offset:    0 Size:     4
//
// }
//
// Resource bind info for meshes
// {
//
//   struct MeshData
//   {
//       
//       struct MeshLOD
//       {
//           
//           uint4 index_buffer;        // Offset:    0
//           uint4 vertex_buffer;       // Offset:   16
//
//       } lod[3];                      // Offset:    0
//       float max_lod_distance[2];     // Offset:   96
//       float mat_val;                 // Offset:  104
//       
//       struct PairUInt16
//       {
//           
//           uint val;                  // Offset:  108
//
//       } texture_indexes;             // Offset:  108
//
//   } $Element;                        // Offset:    0 Size:   112
//
// }
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
// Resource bind info for out_commands
// {
//
//   struct IndirectCommand
//   {
//       
//       float4x4 wvp_mtx;              // Offset:    0
//       uint4 index_buffer;            // Offset:   64
//       uint4 vertex_buffer;           // Offset:   80
//       uint texture_index[2];         // Offset:   96
//       float mat_val;                 // Offset:  104
//       
//       struct DrawIndexedArgs
//       {
//           
//           uint IndexCountPerInstance;// Offset:  108
//           uint InstanceCount;        // Offset:  112
//           uint StartIndexLocation;   // Offset:  116
//           int BaseVertexLocation;    // Offset:  120
//           uint StartInstanceLocation;// Offset:  124
//
//       } draw_arg;                    // Offset:  108
//
//   } $Element;                        // Offset:    0 Size:   128
//
// }
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim      ID      HLSL Bind  Count
// ------------------------------ ---------- ------- ----------- ------- -------------- ------
// meshes                            texture  struct         r/o      T0             t0      1 
// instances                         texture  struct         r/o      T1             t3      1 
// out_commands                          UAV  struct     r/w+cnt      U0             u0      1 
// SceneManagerParams                cbuffer      NA          NA     CB0            cb0      1 
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
dcl_constantbuffer CB0[0:0][1], immediateIndexed, space=0
dcl_resource_structured T0[0:0], 112, space=0
dcl_resource_structured T1[3:3], 40, space=0
dcl_uav_structured_opc U0[0:0], 128, space=0
dcl_input vThreadID.x
dcl_temps 5
dcl_thread_group 64, 1, 1
uge r0.x, vThreadID.x, CB0[0][0].x
if_nz r0.x
  ret 
endif 
ld_structured r0.x, vThreadID.x, l(36), T1[3].xxxx
and r0.x, r0.x, l(0x0000ffff)
ld_structured r1.xyzw, r0.x, l(0), T0[0].xyzw
ld_structured r2.xyzw, r0.x, l(16), T0[0].xyzw
ld_structured r0.xy, r0.x, l(104), T0[0].xyxx
and r3.x, r0.y, l(0x0000ffff)
ushr r3.y, r0.y, l(16)
ushr r3.w, r1.z, l(2)
imm_atomic_alloc r4.x, U0[0]
store_structured U0[0].xyzw, r4.x, l(0), l(0,0,0,0)
store_structured U0[0].xyzw, r4.x, l(16), l(0,0,0,0)
store_structured U0[0].xyzw, r4.x, l(32), l(0,0,0,0)
store_structured U0[0].xyzw, r4.x, l(48), l(0,0,0,0)
store_structured U0[0].xyzw, r4.x, l(64), r1.xyzw
store_structured U0[0].xyzw, r4.x, l(80), r2.xyzw
mov r3.z, r0.x
store_structured U0[0].xyzw, r4.x, l(96), r3.xyzw
store_structured U0[0].xyzw, r4.x, l(112), l(1,0,0,0)
ret 
// Approximately 23 instruction slots used
#endif

const BYTE g_compact_depth_cs[] =
{
     68,  88,  66,  67, 188, 117, 
    253,  61, 112,  35, 142,  10, 
     23, 138, 230, 193,  13, 126, 
    162, 131,   1,   0,   0,   0, 
      0,  12,   0,   0,   5,   0, 
      0,   0,  52,   0,   0,   0, 
    180,   7,   0,   0, 196,   7, 
      0,   0, 212,   7,   0,   0, 
    100,  11,   0,   0,  82,  68, 
     69,  70, 120,   7,   0,   0, 
      4,   0,   0,   0,  16,   1, 
      0,   0,   4,   0,   0,   0, 
     60,   0,   0,   0,   1,   5, 
     83,  67,   0,   5,   0,   0, 
     80,   7,   0,   0,  19,  19, 
     68,  37,  60,   0,   0,   0, 
     24,   0,   0,   0,  40,   0, 
      0,   0,  40,   0,   0,   0, 
     36,   0,   0,   0,  12,   0, 
      0,   0,   0,   0,   0,   0, 
    220,   0,   0,   0,   5,   0, 
      0,   0,   6,   0,   0,   0, 
      1,   0,   0,   0, 112,   0, 
      0,   0,   0,   0,   0,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 227,   0, 
      0,   0,   5,   0,   0,   0, 
      6,   0,   0,   0,   1,   0, 
      0,   0,  40,   0,   0,   0, 
      3,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0, 237,   0,   0,   0, 
     11,   0,   0,   0,   6,   0, 
      0,   0,   1,   0,   0,   0, 
    128,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    250,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      1,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 109, 101, 
    115, 104, 101, 115,   0, 105, 
    110, 115, 116,  97, 110,  99, 
    101, 115,   0, 111, 117, 116, 
     95,  99, 111, 109, 109,  97, 
    110, 100, 115,   0,  83,  99, 
    101, 110, 101,  77,  97, 110, 
     97, 103, 101, 114,  80,  97, 
    114,  97, 109, 115,   0, 171, 
    171, 171, 250,   0,   0,   0, 
      1,   0,   0,   0, 112,   1, 
      0,   0,  16,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 220,   0,   0,   0, 
      1,   0,   0,   0, 208,   1, 
      0,   0, 112,   0,   0,   0, 
      0,   0,   0,   0,   3,   0, 
      0,   0, 227,   0,   0,   0, 
      1,   0,   0,   0, 204,   3, 
      0,   0,  40,   0,   0,   0, 
      0,   0,   0,   0,   3,   0, 
      0,   0, 237,   0,   0,   0, 
      1,   0,   0,   0,  60,   5, 
      0,   0, 128,   0,   0,   0, 
      0,   0,   0,   0,   3,   0, 
      0,   0, 152,   1,   0,   0, 
      0,   0,   0,   0,   4,   0, 
      0,   0,   2,   0,   0,   0, 
    172,   1,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
    105, 110, 115, 116,  97, 110, 
     99, 101, 115,  95, 110, 117, 
    109,   0, 100, 119, 111, 114, 
    100,   0,   0,   0,  19,   0, 
      1,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 166,   1, 
      0,   0, 248,   1,   0,   0, 
      0,   0,   0,   0, 112,   0, 
      0,   0,   2,   0,   0,   0, 
    168,   3,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
     36,  69, 108, 101, 109, 101, 
    110, 116,   0,  77, 101, 115, 
    104,  68,  97, 116,  97,   0, 
    108, 111, 100,   0,  77, 101, 
    115, 104,  76,  79,  68,   0, 
    105, 110, 100, 101, 120,  95, 
     98, 117, 102, 102, 101, 114, 
      0, 117, 105, 110, 116,  52, 
      0, 171, 171, 171,   1,   0, 
     19,   0,   1,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     35,   2,   0,   0, 118, 101, 
    114, 116, 101, 120,  95,  98, 
    117, 102, 102, 101, 114,   0, 
    171, 171,  22,   2,   0,   0, 
     44,   2,   0,   0,   0,   0, 
      0,   0,  80,   2,   0,   0, 
     44,   2,   0,   0,  16,   0, 
      0,   0,   5,   0,   0,   0, 
      1,   0,   8,   0,   3,   0, 
      2,   0,  96,   2,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  14,   2, 
      0,   0, 109,  97, 120,  95, 
    108, 111, 100,  95, 100, 105, 
    115, 116,  97, 110,  99, 101, 
      0, 102, 108, 111,  97, 116, 
      0, 171,   0,   0,   3,   0, 
      1,   0,   1,   0,   2,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 173,   2, 
      0,   0, 109,  97, 116,  95, 
    118,  97, 108,   0,   0,   0, 
      3,   0,   1,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    173,   2,   0,   0, 116, 101, 
    120, 116, 117, 114, 101,  95, 
    105, 110, 100, 101, 120, 101, 
    115,   0,  80,  97, 105, 114, 
     85,  73, 110, 116,  49,  54, 
      0, 118,  97, 108,   0, 171, 
      0,   0,  19,   0,   1,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 166,   1,   0,   0, 
     31,   3,   0,   0,  36,   3, 
      0,   0,   0,   0,   0,   0, 
      5,   0,   0,   0,   1,   0, 
      1,   0,   0,   0,   1,   0, 
     72,   3,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  20,   3,   0,   0, 
     10,   2,   0,   0, 120,   2, 
      0,   0,   0,   0,   0,   0, 
    156,   2,   0,   0, 180,   2, 
      0,   0,  96,   0,   0,   0, 
    216,   2,   0,   0, 224,   2, 
      0,   0, 104,   0,   0,   0, 
      4,   3,   0,   0,  84,   3, 
      0,   0, 108,   0,   0,   0, 
      5,   0,   0,   0,   1,   0, 
     28,   0,   0,   0,   4,   0, 
    120,   3,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   1,   2,   0,   0, 
    248,   1,   0,   0,   0,   0, 
      0,   0,  40,   0,   0,   0, 
      2,   0,   0,   0,  24,   5, 
      0,   0,   0,   0,   0,   0, 
    255, 255, 255, 255,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0,  77, 101, 
    115, 104,  73, 110, 115, 116, 
     97, 110,  99, 101,   0, 116, 
    114,  97, 110, 115, 102, 111, 
    114, 109,   0,  84, 114,  97, 
    110, 115, 102, 111, 114, 109, 
      0, 116, 114,  97, 110, 115, 
    108,  97, 116, 101,   0, 102, 
    108, 111,  97, 116,  51,   0, 
    171, 171,   1,   0,   3,   0, 
      1,   0,   3,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  31,   4, 
      0,   0, 115,  99,  97, 108, 
    101,   0, 114, 111, 116,  97, 
    116, 105, 111, 110,   0, 102, 
    108, 111,  97, 116,  52,   0, 
    171, 171,   1,   0,   3,   0, 
      1,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  91,   4, 
      0,   0,  21,   4,   0,   0, 
     40,   4,   0,   0,   0,   0, 
      0,   0,  76,   4,   0,   0, 
    224,   2,   0,   0,  12,   0, 
      0,   0,  82,   4,   0,   0, 
    100,   4,   0,   0,  16,   0, 
      0,   0,   5,   0,   0,   0, 
      1,   0,   8,   0,   0,   0, 
      3,   0, 136,   4,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  11,   4, 
      0,   0, 114,  97, 100, 105, 
    117, 115,   0, 109, 101, 115, 
    104,  95, 105, 110, 100, 101, 
    120,  95,  97, 110, 100,  95, 
    109,  97, 120,  95, 100, 105, 
    115, 116,  97, 110,  99, 101, 
      0, 171,   1,   4,   0,   0, 
    172,   4,   0,   0,   0,   0, 
      0,   0, 208,   4,   0,   0, 
    224,   2,   0,   0,  32,   0, 
      0,   0, 215,   4,   0,   0, 
     84,   3,   0,   0,  36,   0, 
      0,   0,   5,   0,   0,   0, 
      1,   0,  10,   0,   0,   0, 
      3,   0, 244,   4,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 244,   3, 
      0,   0, 248,   1,   0,   0, 
      0,   0,   0,   0, 128,   0, 
      0,   0,   2,   0,   0,   0, 
     44,   7,   0,   0,   0,   0, 
      0,   0, 255, 255, 255, 255, 
      0,   0,   0,   0, 255, 255, 
    255, 255,   0,   0,   0,   0, 
     73, 110, 100, 105, 114, 101, 
     99, 116,  67, 111, 109, 109, 
     97, 110, 100,   0, 119, 118, 
    112,  95, 109, 116, 120,   0, 
    102, 108, 111,  97, 116,  52, 
    120,  52,   0, 171, 171, 171, 
      3,   0,   3,   0,   4,   0, 
      4,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 124,   5,   0,   0, 
    116, 101, 120, 116, 117, 114, 
    101,  95, 105, 110, 100, 101, 
    120,   0, 171, 171,   0,   0, 
     19,   0,   1,   0,   1,   0, 
      2,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    166,   1,   0,   0, 100, 114, 
     97, 119,  95,  97, 114, 103, 
      0,  68, 114,  97, 119,  73, 
    110, 100, 101, 120, 101, 100, 
     65, 114, 103, 115,   0,  73, 
    110, 100, 101, 120,  67, 111, 
    117, 110, 116,  80, 101, 114, 
     73, 110, 115, 116,  97, 110, 
     99, 101,   0,  73, 110, 115, 
    116,  97, 110,  99, 101,  67, 
    111, 117, 110, 116,   0,  83, 
    116,  97, 114, 116,  73, 110, 
    100, 101, 120,  76, 111,  99, 
     97, 116, 105, 111, 110,   0, 
     66,  97, 115, 101,  86, 101, 
    114, 116, 101, 120,  76, 111, 
     99,  97, 116, 105, 111, 110, 
      0, 105, 110, 116,   0, 171, 
      0,   0,   2,   0,   1,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  67,   6,   0,   0, 
     83, 116,  97, 114, 116,  73, 
    110, 115, 116,  97, 110,  99, 
    101,  76, 111,  99,  97, 116, 
    105, 111, 110,   0, 171, 171, 
    249,   5,   0,   0,  36,   3, 
      0,   0,   0,   0,   0,   0, 
     15,   6,   0,   0,  36,   3, 
      0,   0,   4,   0,   0,   0, 
     29,   6,   0,   0,  36,   3, 
      0,   0,   8,   0,   0,   0, 
     48,   6,   0,   0,  72,   6, 
      0,   0,  12,   0,   0,   0, 
    108,   6,   0,   0,  36,   3, 
      0,   0,  16,   0,   0,   0, 
      5,   0,   0,   0,   1,   0, 
      5,   0,   0,   0,   5,   0, 
    132,   6,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 233,   5,   0,   0, 
    116,   5,   0,   0, 136,   5, 
      0,   0,   0,   0,   0,   0, 
     22,   2,   0,   0,  44,   2, 
      0,   0,  64,   0,   0,   0, 
     80,   2,   0,   0,  44,   2, 
      0,   0,  80,   0,   0,   0, 
    172,   5,   0,   0, 188,   5, 
      0,   0,  96,   0,   0,   0, 
    216,   2,   0,   0, 224,   2, 
      0,   0, 104,   0,   0,   0, 
    224,   5,   0,   0, 192,   6, 
      0,   0, 108,   0,   0,   0, 
      5,   0,   0,   0,   1,   0, 
     32,   0,   0,   0,   6,   0, 
    228,   6,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 100,   5,   0,   0, 
     77, 105,  99, 114, 111, 115, 
    111, 102, 116,  32,  40,  82, 
     41,  32,  72,  76,  83,  76, 
     32,  83, 104,  97, 100, 101, 
    114,  32,  67, 111, 109, 112, 
    105, 108, 101, 114,  32,  49, 
     48,  46,  49,   0,  73,  83, 
     71,  78,   8,   0,   0,   0, 
      0,   0,   0,   0,   8,   0, 
      0,   0,  79,  83,  71,  78, 
      8,   0,   0,   0,   0,   0, 
      0,   0,   8,   0,   0,   0, 
     83,  72,  69,  88, 136,   3, 
      0,   0,  81,   0,   5,   0, 
    226,   0,   0,   0, 106,   8, 
      0,   1,  89,   0,   0,   7, 
     70, 142,  48,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
    162,   0,   0,   7,  70, 126, 
     48,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 112,   0,   0,   0, 
      0,   0,   0,   0, 162,   0, 
      0,   7,  70, 126,  48,   0, 
      1,   0,   0,   0,   3,   0, 
      0,   0,   3,   0,   0,   0, 
     40,   0,   0,   0,   0,   0, 
      0,   0, 158,   0, 128,   7, 
     70, 238,  49,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 128,   0, 
      0,   0,   0,   0,   0,   0, 
     95,   0,   0,   2,  18,   0, 
      2,   0, 104,   0,   0,   2, 
      5,   0,   0,   0, 155,   0, 
      0,   4,  64,   0,   0,   0, 
      1,   0,   0,   0,   1,   0, 
      0,   0,  80,   0,   0,   8, 
     18,   0,  16,   0,   0,   0, 
      0,   0,  10,   0,   2,   0, 
     10, 128,  48,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  31,   0, 
      4,   3,  10,   0,  16,   0, 
      0,   0,   0,   0,  62,   0, 
      0,   1,  21,   0,   0,   1, 
    167,   0,   0,   9,  18,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,   2,   0,   1,  64, 
      0,   0,  36,   0,   0,   0, 
      6, 112,  32,   0,   1,   0, 
      0,   0,   3,   0,   0,   0, 
      1,   0,   0,   7,  18,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
    255, 255,   0,   0, 167,   0, 
      0,  10, 242,   0,  16,   0, 
      1,   0,   0,   0,  10,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  70, 126,  32,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 167,   0,   0,  10, 
    242,   0,  16,   0,   2,   0, 
      0,   0,  10,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,  16,   0,   0,   0, 
     70, 126,  32,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    167,   0,   0,  10,  50,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   0,   0, 
      0,   0,   1,  64,   0,   0, 
    104,   0,   0,   0,  70, 112, 
     32,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   7,  18,   0,  16,   0, 
      3,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0, 255, 255, 
      0,   0,  85,   0,   0,   7, 
     34,   0,  16,   0,   3,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,  16,   0,   0,   0, 
     85,   0,   0,   7, 130,   0, 
     16,   0,   3,   0,   0,   0, 
     42,   0,  16,   0,   1,   0, 
      0,   0,   1,  64,   0,   0, 
      2,   0,   0,   0, 178,   0, 
      0,   6,  18,   0,  16,   0, 
      4,   0,   0,   0,   0, 224, 
     33,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 168,   0, 
      0,  13, 242, 224,  33,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      4,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
      2,  64,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 168,   0,   0,  13, 
    242, 224,  33,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   4,   0, 
      0,   0,   1,  64,   0,   0, 
     16,   0,   0,   0,   2,  64, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
    168,   0,   0,  13, 242, 224, 
     33,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   4,   0,   0,   0, 
      1,  64,   0,   0,  32,   0, 
      0,   0,   2,  64,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0, 168,   0, 
      0,  13, 242, 224,  33,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      4,   0,   0,   0,   1,  64, 
      0,   0,  48,   0,   0,   0, 
      2,  64,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0, 168,   0,   0,  10, 
    242, 224,  33,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   4,   0, 
      0,   0,   1,  64,   0,   0, 
     64,   0,   0,   0,  70,  14, 
     16,   0,   1,   0,   0,   0, 
    168,   0,   0,  10, 242, 224, 
     33,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   4,   0,   0,   0, 
      1,  64,   0,   0,  80,   0, 
      0,   0,  70,  14,  16,   0, 
      2,   0,   0,   0,  54,   0, 
      0,   5,  66,   0,  16,   0, 
      3,   0,   0,   0,  10,   0, 
     16,   0,   0,   0,   0,   0, 
    168,   0,   0,  10, 242, 224, 
     33,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   4,   0,   0,   0, 
      1,  64,   0,   0,  96,   0, 
      0,   0,  70,  14,  16,   0, 
      3,   0,   0,   0, 168,   0, 
      0,  13, 242, 224,  33,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      4,   0,   0,   0,   1,  64, 
      0,   0, 112,   0,   0,   0, 
      2,  64,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  62,   0,   0,   1, 
     83,  84,  65,  84, 148,   0, 
      0,   0,  23,   0,   0,   0, 
      5,   0,   0,   0,   0,   0, 
      0,   0,   1,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   5,   0,   0,   0, 
      2,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   4,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   1,   0, 
      0,   0,   8,   0,   0,   0
};