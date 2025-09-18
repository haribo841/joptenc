/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "../src/xJPEG_Arithmetic.h"
#include "xByteBuffer.h"
#include "xBitstream.h"
#include "xTestUtils.h"
#include <array>
#include <iomanip> // For std::hex
#include <iostream>
#include <string>
#include <vector>

using namespace PMBB_NAMESPACE::JPEG;
using namespace PMBB_NAMESPACE;

//===============================================================================================================================================================================================================

static constexpr int32 c_NumUnits = 64 * 1024;

//===============================================================================================================================================================================================================

static constexpr uint32 UND = std::numeric_limits<uint32>::max();

struct tTraceEntry
{
    uint32 D, MPS, CX, Qe, A, C, CT, ST;
};

static const std::array<tTraceEntry, 257> EncoderTrace =
{ {
        //  D MPS CX     Qe        A          C  CT ST
          { 0, 0, 0, 0x5A1D, 0x0000, 0x00000000, 11, 0, },
          { 0, 0, 1, 0x5A1D, 0xA5E3, 0x00000000, 11, 0, },//0
          { 0, 0, 0, 0x2586, 0xB43A, 0x0000978C, 10, 0, },//1
          { 0, 0, 0, 0x2586, 0x8EB4, 0x0000978C, 10, 0, },//2
          { 0, 0, 0, 0x1114, 0xD25C, 0x00012F18,  9, 0, },//3
          { 0, 0, 0, 0x1114, 0xC148, 0x00012F18,  9, 0, },//4
          { 0, 0, 0, 0x1114, 0xB034, 0x00012F18,  9, 0, },//5
          { 0, 0, 0, 0x1114, 0x9F20, 0x00012F18,  9, 0, },//6
          { 0, 0, 0, 0x1114, 0x8E0C, 0x00012F18,  9, 0, },//7
          { 0, 0, 0, 0x080B, 0xF9F0, 0x00025E30,  8, 0, },//8
          { 0, 0, 0, 0x080B, 0xF1E5, 0x00025E30,  8, 0, },//9
          { 0, 0, 0, 0x080B, 0xE9DA, 0x00025E30,  8, 0, },//10
          { 0, 0, 0, 0x080B, 0xE1CF, 0x00025E30,  8, 0, },//11
          { 0, 0, 0, 0x080B, 0xD9C4, 0x00025E30,  8, 0, },//12
          { 1, 0, 0, 0x080B, 0xD1B9, 0x00025E30,  8, 0, },//13
          { 0, 0, 0, 0x17B9, 0x80B0, 0x00327DE0,  4, 0, },//14
          { 0, 0, 0, 0x1182, 0xD1EE, 0x0064FBC0,  3, 0, },//15
          { 0, 0, 0, 0x1182, 0xC06C, 0x0064FBC0,  3, 0, },//16
          { 0, 0, 0, 0x1182, 0xAEEA, 0x0064FBC0,  3, 0, },//17
          { 0, 0, 0, 0x1182, 0x9D68, 0x0064FBC0,  3, 0, },//18
          { 0, 0, 0, 0x1182, 0x8BE6, 0x0064FBC0,  3, 0, },//19
          { 0, 0, 0, 0x0CEF, 0xF4C8, 0x00C9F780,  2, 0, },//20
          { 0, 0, 0, 0x0CEF, 0xE7D9, 0x00C9F780,  2, 0, },//21
          { 0, 0, 0, 0x0CEF, 0xDAEA, 0x00C9F780,  2, 0, },//22
          { 0, 0, 0, 0x0CEF, 0xCDFB, 0x00C9F780,  2, 0, },//23
          { 1, 0, 0, 0x0CEF, 0xC10C, 0x00C9F780,  2, 0, },//24
          { 0, 0, 0, 0x1518, 0xCEF0, 0x000AB9D0,  6, 0, },//25
          { 1, 0, 0, 0x1518, 0xB9D8, 0x000AB9D0,  6, 0, },
          { 0, 0, 0, 0x1AA9, 0xA8C0, 0x005AF480,  3, 0, },
          { 0, 0, 0, 0x1AA9, 0x8E17, 0x005AF480,  3, 0, },
          { 0, 0, 0, 0x174E, 0xE6DC, 0x00B5E900,  2, 0, },
          { 1, 0, 0, 0x174E, 0xCF8E, 0x00B5E900,  2, 0, },
          { 0, 0, 0, 0x1AA9, 0xBA70, 0x00050A00,  7, 0, },
          { 0, 0, 0, 0x1AA9, 0x9FC7, 0x00050A00,  7, 0, },
          { 0, 0, 0, 0x1AA9, 0x851E, 0x00050A00,  7, 0, },
          { 0, 0, 0, 0x174E, 0xD4EA, 0x000A1400,  6, 0, },
          { 0, 0, 0, 0x174E, 0xBD9C, 0x000A1400,  6, 0, },
          { 0, 0, 0, 0x174E, 0xA64E, 0x000A1400,  6, 0, },
          { 0, 0, 0, 0x174E, 0x8F00, 0x000A1400,  6, 0, },
          { 0, 0, 0, 0x1424, 0xEF64, 0x00142800,  5, 0, },
          { 0, 0, 0, 0x1424, 0xDB40, 0x00142800,  5, 0, },
          { 0, 0, 0, 0x1424, 0xC71C, 0x00142800,  5, 0, },
          { 0, 0, 0, 0x1424, 0xB2F8, 0x00142800,  5, 0, },
          { 0, 0, 0, 0x1424, 0x9ED4, 0x00142800,  5, 0, },
          { 0, 0, 0, 0x1424, 0x8AB0, 0x00142800,  5, 0, },
          { 0, 0, 0, 0x119C, 0xED18, 0x00285000,  4, 0, },
          { 0, 0, 0, 0x119C, 0xDB7C, 0x00285000,  4, 0, },
          { 0, 0, 0, 0x119C, 0xC9E0, 0x00285000,  4, 0, },
          { 0, 0, 0, 0x119C, 0xB844, 0x00285000,  4, 0, },
          { 0, 0, 0, 0x119C, 0xA6A8, 0x00285000,  4, 0, },
          { 0, 0, 0, 0x119C, 0x950C, 0x00285000,  4, 0, },
          { 0, 0, 0, 0x119C, 0x8370, 0x00285000,  4, 0, },
          { 0, 0, 0, 0x0F6B, 0xE3A8, 0x0050A000,  3, 0, },
          { 0, 0, 0, 0x0F6B, 0xD43D, 0x0050A000,  3, 0, },
          { 0, 0, 0, 0x0F6B, 0xC4D2, 0x0050A000,  3, 0, },
          { 0, 0, 0, 0x0F6B, 0xB567, 0x0050A000,  3, 0, },
          { 1, 0, 0, 0x0F6B, 0xA5FC, 0x0050A000,  3, 0, },
          { 1, 0, 0, 0x1424, 0xF6B0, 0x00036910,  7, 0, },
          { 0, 0, 0, 0x1AA9, 0xA120, 0x00225CE0,  4, 0, },
          { 0, 0, 0, 0x1AA9, 0x8677, 0x00225CE0,  4, 0, },
          { 0, 0, 0, 0x174E, 0xD79C, 0x0044B9C0,  3, 0, },
          { 0, 0, 0, 0x174E, 0xC04E, 0x0044B9C0,  3, 0, },
          { 0, 0, 0, 0x174E, 0xA900, 0x0044B9C0,  3, 0, },
          { 0, 0, 0, 0x174E, 0x91B2, 0x0044B9C0,  3, 0, },
          { 0, 0, 0, 0x1424, 0xF4C8, 0x00897380,  2, 0, },
          { 0, 0, 0, 0x1424, 0xE0A4, 0x00897380,  2, 0, },
          { 0, 0, 0, 0x1424, 0xCC80, 0x00897380,  2, 0, },
          { 0, 0, 0, 0x1424, 0xB85C, 0x00897380,  2, 0, },
          { 0, 0, 0, 0x1424, 0xA438, 0x00897380,  2, 0, },
          { 0, 0, 0, 0x1424, 0x9014, 0x00897380,  2, 0, },
          { 1, 0, 0, 0x119C, 0xF7E0, 0x0112E700,  1, 0, },
          { 1, 0, 0, 0x1424, 0x8CE0, 0x001E6A20,  6, 0, },
          { 0, 0, 0, 0x1AA9, 0xA120, 0x00F716E0,  3, 0, },
          { 1, 0, 0, 0x1AA9, 0x8677, 0x00F716E0,  3, 0, },
          { 0, 0, 0, 0x2516, 0xD548, 0x00041570,  8, 0, },
          { 1, 0, 0, 0x2516, 0xB032, 0x00041570,  8, 0, },
          { 0, 0, 0, 0x299A, 0x9458, 0x00128230,  6, 0, },
          { 0, 0, 0, 0x2516, 0xD57C, 0x00250460,  5, 0, },
          { 1, 0, 0, 0x2516, 0xB066, 0x00250460,  5, 0, },
          { 0, 0, 0, 0x299A, 0x9458, 0x00963EC0,  3, 0, },
          { 1, 0, 0, 0x2516, 0xD57C, 0x012C7D80,  2, 0, },
          { 0, 0, 0, 0x299A, 0x9458, 0x0004B798,  8, 0, },
          { 0, 0, 0, 0x2516, 0xD57C, 0x00096F30,  7, 0, },
          { 0, 0, 0, 0x2516, 0xB066, 0x00096F30,  7, 0, },
          { 0, 0, 0, 0x2516, 0x8B50, 0x00096F30,  7, 0, },
          { 1, 0, 0, 0x1EDF, 0xCC74, 0x0012DE60,  6, 0, },
          { 1, 0, 0, 0x2516, 0xF6F8, 0x009C5FA8,  3, 0, },
          { 1, 0, 0, 0x299A, 0x9458, 0x0274C628,  1, 0, },
          { 0, 0, 0, 0x32B4, 0xA668, 0x0004C398,  7, 0, },
          { 0, 0, 0, 0x2E17, 0xE768, 0x00098730,  6, 0, },
          { 1, 0, 0, 0x2E17, 0xB951, 0x00098730,  6, 0, },
          { 0, 0, 0, 0x32B4, 0xB85C, 0x002849A8,  4, 0, },
          { 1, 0, 0, 0x32B4, 0x85A8, 0x002849A8,  4, 0, },
          { 0, 0, 0, 0x3C3D, 0xCAD0, 0x00A27270,  2, 0, },
          { 1, 0, 0, 0x3C3D, 0x8E93, 0x00A27270,  2, 0, },
          { 0, 0, 0, 0x415E, 0xF0F4, 0x00031318,  8, 0, },
          { 1, 0, 0, 0x415E, 0xAF96, 0x00031318,  8, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x000702A0,  7, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x000E7E46,  6, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x001D92B4,  5, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x003B9E6E,  4, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x0077D304,  3, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x00F01F0E,  2, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x01E0D444,  1, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x0002218E,  8, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x0004D944,  7, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x000A2B8E,  6, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x0014ED44,  5, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x002A538E,  4, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x00553D44,  3, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x00AAF38E,  2, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x01567D44,  1, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x0005738E,  8, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x000B7D44,  7, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x0017738E,  6, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x002F7D44,  5, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x005F738E,  4, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x00BF7D44,  3, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x017F738E,  2, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x02FF7D44,  1, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x0007738E,  8, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x000F7D44,  7, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x001F738E,  6, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x003F7D44,  5, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x007F738E,  4, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x00FF7D44,  3, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x01FF738E,  2, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x03FF7D44,  1, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x0007738E,  8, 1, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x000F7D44,  7, 1, },
          { 0, 0, 0, 0x415E, 0x8C72, 0x001F738E,  6, 1, },
          { 0, 0, 0, 0x3C3D, 0x9628, 0x003EE71C,  5, 1, },
          { 0, 0, 0, 0x375E, 0xB3D6, 0x007DCE38,  4, 1, },
          { 0, 0, 0, 0x32B4, 0xF8F0, 0x00FB9C70,  3, 1, },
          { 1, 0, 0, 0x32B4, 0xC63C, 0x00FB9C70,  3, 1, },
          { 0, 0, 0, 0x3C3D, 0xCAD0, 0x03F0BFE0,  1, 1, },
          { 1, 0, 0, 0x3C3D, 0x8E93, 0x03F0BFE0,  1, 1, },
          { 1, 0, 0, 0x415E, 0xF0F4, 0x000448D8,  7, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x0009F0DC,  6, 0, },
          { 0, 0, 0, 0x415E, 0x8C72, 0x00145ABE,  5, 0, },
          { 0, 0, 0, 0x3C3D, 0x9628, 0x0028B57C,  4, 0, },
          { 0, 0, 0, 0x375E, 0xB3D6, 0x00516AF8,  3, 0, },
          { 0, 0, 0, 0x32B4, 0xF8F0, 0x00A2D5F0,  2, 0, },
          { 0, 0, 0, 0x32B4, 0xC63C, 0x00A2D5F0,  2, 0, },
          { 0, 0, 0, 0x32B4, 0x9388, 0x00A2D5F0,  2, 0, },
          { 0, 0, 0, 0x2E17, 0xC1A8, 0x0145ABE0,  1, 0, },
          { 1, 0, 0, 0x2E17, 0x9391, 0x0145ABE0,  1, 0, },
          { 0, 0, 0, 0x32B4, 0xB85C, 0x00084568,  7, 0, },
          { 0, 0, 0, 0x32B4, 0x85A8, 0x00084568,  7, 0, },
          { 0, 0, 0, 0x2E17, 0xA5E8, 0x00108AD0,  6, 0, },
          { 0, 0, 0, 0x299A, 0xEFA2, 0x002115A0,  5, 0, },
          { 0, 0, 0, 0x299A, 0xC608, 0x002115A0,  5, 0, },
          { 0, 0, 0, 0x299A, 0x9C6E, 0x002115A0,  5, 0, },
          { 0, 0, 0, 0x2516, 0xE5A8, 0x00422B40,  4, 0, },
          { 0, 0, 0, 0x2516, 0xC092, 0x00422B40,  4, 0, },
          { 0, 0, 0, 0x2516, 0x9B7C, 0x00422B40,  4, 0, },
          { 0, 0, 0, 0x1EDF, 0xECCC, 0x00845680,  3, 0, },
          { 0, 0, 0, 0x1EDF, 0xCDED, 0x00845680,  3, 0, },
          { 0, 0, 0, 0x1EDF, 0xAF0E, 0x00845680,  3, 0, },
          { 0, 0, 0, 0x1EDF, 0x902F, 0x00845680,  3, 0, },
          { 1, 0, 0, 0x1AA9, 0xE2A0, 0x0108AD00,  2, 0, },
          { 1, 0, 0, 0x2516, 0xD548, 0x000BA7B8,  7, 0, },
          { 1, 0, 0, 0x299A, 0x9458, 0x00315FA8,  5, 0, },
          { 1, 0, 0, 0x32B4, 0xA668, 0x00C72998,  3, 0, },
          { 1, 0, 0, 0x3C3D, 0xCAD0, 0x031E7530,  1, 0, },
          { 1, 0, 0, 0x415E, 0xF0F4, 0x000C0F0C,  7, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x00197D44,  6, 0, },
          { 0, 0, 0, 0x415E, 0x8C72, 0x0033738E,  5, 0, },
          { 1, 0, 0, 0x3C3D, 0x9628, 0x0066E71C,  4, 0, },
          { 1, 0, 0, 0x415E, 0xF0F4, 0x019D041C,  2, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x033B6764,  1, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x000747CE,  8, 0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x000F25C4,  7, 0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x001EC48E,  6, 0, },
          { 1, 0, 1, 0x4639, 0x82BC, 0x003E1F44,  5, 0, },
          { 1, 0, 0, 0x4B85, 0xF20C, 0x00F87D10,  3, 0, },
          { 1, 0, 1, 0x504F, 0x970A, 0x01F2472E,  2, 0, },
          { 0, 0, 1, 0x5522, 0x8D76, 0x03E48E5C,  1, 0, },
          { 0, 0, 0, 0x504F, 0xAA44, 0x00018D60,  8, 0, },
          { 1, 0, 0, 0x4B85, 0xB3EA, 0x00031AC0,  7, 0, },
          { 1, 0, 1, 0x504F, 0x970A, 0x0007064A,  6, 0, },
          { 1, 0, 1, 0x5522, 0x8D76, 0x000E0C94,  5, 0, },
          { 1, 0, 0, 0x59EB, 0xE150, 0x00383250,  3, 0, },
          { 0, 1, 0, 0x59EB, 0xB3D6, 0x0071736A,  2, 0, },
          { 1, 0, 0, 0x59EB, 0xB3D6, 0x00E39AAA,  1, 0, },
          { 1, 1, 0, 0x59EB, 0xB3D6, 0x0007E92A,  8, 0, },
          { 1, 1, 0, 0x5522, 0xB3D6, 0x000FD254,  7, 0, },
          { 1, 1, 0, 0x504F, 0xBD68, 0x001FA4A8,  6, 0, },
          { 0, 1, 0, 0x4B85, 0xDA32, 0x003F4950,  5, 0, },
          { 1, 1, 1, 0x504F, 0x970A, 0x007FAFFA,  4, 0, },
          { 1, 1, 0, 0x4B85, 0xA09E, 0x00FFED6A,  3, 0, },
          { 0, 1, 0, 0x4639, 0xAA32, 0x01FFDAD4,  2, 0, },
          { 0, 1, 1, 0x4B85, 0x8C72, 0x04007D9A,  1, 0, },
          { 1, 1, 1, 0x504F, 0x81DA, 0x0000FB34,  8, 0, },
          { 1, 1, 0, 0x4B85, 0xA09E, 0x0002597E,  7, 0, },
          { 1, 1, 0, 0x4639, 0xAA32, 0x0004B2FC,  6, 0, },
          { 0, 1, 0, 0x415E, 0xC7F2, 0x000965F8,  5, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x0013D918,  4, 0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x00282B36,  3, 0, },
          { 0, 1, 1, 0x4639, 0x82BC, 0x0050EC94,  2, 0, },
          { 1, 1, 0, 0x4B85, 0xF20C, 0x0003B250,  8, 0, },
          { 1, 1, 0, 0x4B85, 0xA687, 0x0003B250,  8, 0, },
          { 1, 1, 0, 0x4639, 0xB604, 0x000764A0,  7, 0, },
          { 0, 1, 0, 0x415E, 0xDF96, 0x000EC940,  6, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x001ECEF0,  5, 0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x003E16E6,  4, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x007CC3F4,  3, 0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x00FA00EE,  2, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x01F49804,  1, 0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x0001A90E,  8, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x0003E844,  7, 0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x0008498E,  6, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x00112944,  5, 0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x0022CB8E,  4, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x00462D44,  3, 0, },
          { 1, 1, 0, 0x415E, 0x8C72, 0x008CD38E,  2, 0, },
          { 1, 1, 0, 0x3C3D, 0x9628, 0x0119A71C,  1, 0, },
          { 1, 1, 0, 0x375E, 0xB3D6, 0x00034E38,  8, 0, },
          { 1, 1, 0, 0x32B4, 0xF8F0, 0x00069C70,  7, 0, },
          { 1, 1, 0, 0x32B4, 0xC63C, 0x00069C70,  7, 0, },
          { 0, 1, 0, 0x32B4, 0x9388, 0x00069C70,  7, 0, },
          { 1, 1, 0, 0x3C3D, 0xCAD0, 0x001BF510,  5, 0, },
          { 1, 1, 0, 0x3C3D, 0x8E93, 0x001BF510,  5, 0, },
          { 1, 1, 0, 0x375E, 0xA4AC, 0x0037EA20,  4, 0, },
          { 0, 1, 0, 0x32B4, 0xDA9C, 0x006FD440,  3, 0, },
          { 1, 1, 0, 0x3C3D, 0xCAD0, 0x01C1F0A0,  1, 0, },
          { 1, 1, 0, 0x3C3D, 0x8E93, 0x01C1F0A0,  1, 0, },
          { 0, 1, 0, 0x375E, 0xA4AC, 0x0003E140,  8, 0, },
          { 1, 1, 0, 0x3C3D, 0xDD78, 0x00113A38,  6, 0, },
          { 0, 1, 0, 0x3C3D, 0xA13B, 0x00113A38,  6, 0, },
          { 0, 1, 0, 0x415E, 0xF0F4, 0x00467CD8,  4, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x008E58DC,  3, 0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x011D2ABE,  2, 0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x023AEBA4,  1, 0, },
          { 1, 1, 0, 0x415E, 0x8C72, 0x0006504E,  8, 0, },
          { 1, 1, 0, 0x3C3D, 0x9628, 0x000CA09C,  7, 0, },
          { 1, 1, 0, 0x375E, 0xB3D6, 0x00194138,  6, 0, },
          { 1, 1, 0, 0x32B4, 0xF8F0, 0x00328270,  5, 0, },
          { 1, 1, 0, 0x32B4, 0xC63C, 0x00328270,  5, 0, },
          { 0, 1, 0, 0x32B4, 0x9388, 0x00328270,  5, 0, },
          { 1, 1, 0, 0x3C3D, 0xCAD0, 0x00CB8D10,  3, 0, },
          { 1, 1, 0, 0x3C3D, 0x8E93, 0x00CB8D10,  3, 0, },
          { 1, 1, 0, 0x375E, 0xA4AC, 0x01971A20,  2, 0, },
          { 0, 1, 0, 0x32B4, 0xDA9C, 0x032E3440,  1, 0, },
          { 0, 1, 0, 0x3C3D, 0xCAD0, 0x000B70A0,  7, 0, },
          { 1, 1, 0, 0x415E, 0xF0F4, 0x002FFCCC,  5, 0, },
          { 1, 1, 0, 0x415E, 0xAF96, 0x002FFCCC,  5, 0, },
          { 1, 1, 0, 0x3C3D, 0xDC70, 0x005FF998,  4, 0, },
          { 0, 1, 0, 0x3C3D, 0xA033, 0x005FF998,  4, 0, },
          { 1, 1, 0, 0x415E, 0xF0F4, 0x01817638,  2, 0, },
          { 0, 1, 0, 0x415E, 0xAF96, 0x01817638,  2, 0, },
          { 0, 1, 1, 0x4639, 0x82BC, 0x0303C8E0,  1, 0, },
          { 1, 1, 0, 0x4B85, 0xF20C, 0x000F2380,  7, 0, },
          { 1, 1, 0, 0x4B85, 0xA687, 0x000F2380,  7, 0, },
          { 0, 1, 0, 0x4639, 0xB604, 0x001E4700,  6, 0, },
          { 0, 1, 1, 0x4B85, 0x8C72, 0x003D6D96,  5, 0, },
          { UND, UND, 0, UND, 0x81DA, 0x007ADB2C,  4, 0, }, //post flush
        } };

#ifndef PMBB_JPEG_ARITHM_AVOID_MARKER_EMULATION
#define PMBB_JPEG_ARITHM_AVOID_MARKER_EMULATION 1
#endif

#if PMBB_JPEG_ARITHM_AVOID_MARKER_EMULATION
static const std::array<uint8, 29> EncodedBytes = { 0x65, 0x5B, 0x51, 0x44, 0xF7, 0x96, 0x9D, 0x51, 0x78, 0x55, 0xBF, 0xFF, 0x00, 0xFC, 0x51, 0x84, 0xC7, 0xCE, 0xF9, 0x39, 0x00, 0x28, 0x7D, 0x46, 0x70, 0x8E, 0xCB, 0xC0, 0xF6 };
#else //PMBB_JPEG_ARITHM_AVOID_MARKER_EMULATION
static const std::array<uint8, 28> EncodedBytes = { 0x65, 0x5B, 0x51, 0x44, 0xF7, 0x96, 0x9D, 0x51, 0x78, 0x55, 0xBF, 0xFF, 0xFC, 0x51, 0x84, 0xC7, 0xCE, 0xF9, 0x39, 0x00, 0x28, 0x7D, 0x46, 0x70, 0x8E, 0xCB, 0xC0, 0xF6 };
#endif //PMBB_JPEG_ARITHM_AVOID_MARKER_EMULATION

void checkEncoder(const xArithCoreModel& Model, const xArithCoreEnc& Enc, const tTraceEntry& TestSet)
{
    // Getting current values ​​from the encoder and model
    uint32_t TstProbIdx = Model.getProbIndex();
    uint32_t TstMPS = Model.getMPS();
    uint32_t TstQe = Model.getQe();
    uint32_t TstC = Enc.getC();
    uint32_t TstA = Enc.getA();
    uint32_t TstCT = Enc.getCT();
    uint32_t TstST = Enc.getST();

    // Retrieving expected values ​​from the TestSet structure via named fields
    uint32_t D = TestSet.D;
    uint32_t MPS = TestSet.MPS;
    //uint32_t CX = TestSet.CX;
    uint32_t Qe = TestSet.Qe;
    uint32_t A = TestSet.A;
    uint32_t C = TestSet.C;
    uint32_t CT = TestSet.CT;
    uint32_t ST = TestSet.ST;

    // Validation of the A value
    bool CorrectA = TstA <= 0x10000 && TstA > 0x7FFF;
    CHECK(CorrectA);

    // Mask for A because the test vector only stores 16 bits
    TstA = TstA & 0xFFFF;

    // Comparison with expected values ​​(if not UNDEFINED)
    if (MPS != UND) { CHECK(TstMPS == MPS); }
    if (Qe != UND) { CHECK(TstQe == Qe); }
    if (A != UND) { CHECK(TstA == A); }
    if (C != UND) { CHECK(TstC == C); }
    if (CT != UND) { CHECK(TstCT == CT); }
    if (ST != UND) { CHECK(TstST == ST); }
}

void testEnc()
{
    xByteBuffer Buff(4096);
    xBitstreamWriter Writer;
    Writer.bindByteBuffer(&Buff);
    xArithCoreModel Model;
    Model.init();
    xArithCoreEnc Enc(Writer);
    Enc.initialize();

    for (int32 i = 0; i < EncoderTrace.size() - 1; i++)
    {
        // This function checks the state BEFORE encoding and throws an exception on error
        checkEncoder(Model, Enc, EncoderTrace[i]);

        // Gets the symbol to encode from the current trace step
        const uint32 D = EncoderTrace[i].D;

        // An encoding operation that changes the internal state of the Model and Enc
        Enc.encodeBinMP(D, Model);
    }

    // Final check of the final state (before calling finish())
    checkEncoder(Model, Enc, EncoderTrace.back());

    Enc.finish();

    // The rest of the test remains unchanged
    uint32 NumBytesWritten = Buff.getDataSize();
    CHECK(NumBytesWritten == EncodedBytes.size());

    for (size_t i = 0; i < EncodedBytes.size(); i++)
    {
        uint8 EncByte = Buff.getReadPtr()[i];
        uint8 RefByte = EncodedBytes[i];
        CHECK(EncByte == RefByte);
    }

    std::cout << ">>> Encoder test completed successfully <<<\n";
}

// Helper to print vector contents in hex for better debugging
namespace doctest {
    template <typename T>
    struct StringMaker<std::vector<T>> {
        static String convert(const std::vector<T>& v) {
            std::ostringstream oss;
            oss << "{ ";
            for (const auto& item : v) {
                oss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(item) << " ";
            }
            oss << "}";
            return oss.str().c_str();
        }
    };
}

//===============================================================================================================================================================================================================

static const std::array<tTraceEntry, 256> DecoderTrace =
{ {
        //  D MPS CX     Qe       A           C  CT NULL
          { 0, 0, 0, 0x5A1D, 0x0000, 0x655B0000,  0,   0, },
          { 0, 0, 1, 0x5A1D, 0xA5E3, 0x655B0000,  0,   0, },
          { 0, 0, 0, 0x2586, 0xB43A, 0x332AA200,  7,   0, },
          { 0, 0, 0, 0x2586, 0x8EB4, 0x332AA200,  7,   0, },
          { 0, 0, 0, 0x1114, 0xD25C, 0x66554400,  6,   0, },
          { 0, 0, 0, 0x1114, 0xC148, 0x66554400,  6,   0, },
          { 0, 0, 0, 0x1114, 0xB034, 0x66554400,  6,   0, },
          { 0, 0, 0, 0x1114, 0x9F20, 0x66554400,  6,   0, },
          { 0, 0, 0, 0x1114, 0x8E0C, 0x66554400,  6,   0, },
          { 0, 0, 0, 0x080B, 0xF9F0, 0xCCAA8800,  5,   0, },
          { 0, 0, 0, 0x080B, 0xF1E5, 0xCCAA8800,  5,   0, },
          { 0, 0, 0, 0x080B, 0xE9DA, 0xCCAA8800,  5,   0, },
          { 0, 0, 0, 0x080B, 0xE1CF, 0xCCAA8800,  5,   0, },
          { 0, 0, 0, 0x080B, 0xD9C4, 0xCCAA8800,  5,   0, },
          { 1, 0, 0, 0x080B, 0xD1B9, 0xCCAA8800,  5,   0, },
          { 0, 0, 0, 0x17B9, 0x80B0, 0x2FC88000,  1,   0, },
          { 0, 0, 0, 0x1182, 0xD1EE, 0x5F910000,  0,   0, },
          { 0, 0, 0, 0x1182, 0xC06C, 0x5F910000,  0,   0, },
          { 0, 0, 0, 0x1182, 0xAEEA, 0x5F910000,  0,   0, },
          { 0, 0, 0, 0x1182, 0x9D68, 0x5F910000,  0,   0, },
          { 0, 0, 0, 0x1182, 0x8BE6, 0x5F910000,  0,   0, },
          { 0, 0, 0, 0x0CEF, 0xF4C8, 0xBF228800,  7,   0, },
          { 0, 0, 0, 0x0CEF, 0xE7D9, 0xBF228800,  7,   0, },
          { 0, 0, 0, 0x0CEF, 0xDAEA, 0xBF228800,  7,   0, },
          { 0, 0, 0, 0x0CEF, 0xCDFB, 0xBF228800,  7,   0, },
          { 1, 0, 0, 0x0CEF, 0xC10C, 0xBF228800,  7,   0, },
          { 0, 0, 0, 0x1518, 0xCEF0, 0xB0588000,  3,   0, },
          { 1, 0, 0, 0x1518, 0xB9D8, 0xB0588000,  3,   0, },
          { 0, 0, 0, 0x1AA9, 0xA8C0, 0x5CC40000,  0,   0, },
          { 0, 0, 0, 0x1AA9, 0x8E17, 0x5CC40000,  0,   0, },
          { 0, 0, 0, 0x174E, 0xE6DC, 0xB989EE00,  7,   0, },
          { 1, 0, 0, 0x174E, 0xCF8E, 0xB989EE00,  7,   0, },
          { 0, 0, 0, 0x1AA9, 0xBA70, 0x0A4F7000,  4,   0, },
          { 0, 0, 0, 0x1AA9, 0x9FC7, 0x0A4F7000,  4,   0, },
          { 0, 0, 0, 0x1AA9, 0x851E, 0x0A4F7000,  4,   0, },
          { 0, 0, 0, 0x174E, 0xD4EA, 0x149EE000,  3,   0, },
          { 0, 0, 0, 0x174E, 0xBD9C, 0x149EE000,  3,   0, },
          { 0, 0, 0, 0x174E, 0xA64E, 0x149EE000,  3,   0, },
          { 0, 0, 0, 0x174E, 0x8F00, 0x149EE000,  3,   0, },
          { 0, 0, 0, 0x1424, 0xEF64, 0x293DC000,  2,   0, },
          { 0, 0, 0, 0x1424, 0xDB40, 0x293DC000,  2,   0, },
          { 0, 0, 0, 0x1424, 0xC71C, 0x293DC000,  2,   0, },
          { 0, 0, 0, 0x1424, 0xB2F8, 0x293DC000,  2,   0, },
          { 0, 0, 0, 0x1424, 0x9ED4, 0x293DC000,  2,   0, },
          { 0, 0, 0, 0x1424, 0x8AB0, 0x293DC000,  2,   0, },
          { 0, 0, 0, 0x119C, 0xED18, 0x527B8000,  1,   0, },
          { 0, 0, 0, 0x119C, 0xDB7C, 0x527B8000,  1,   0, },
          { 0, 0, 0, 0x119C, 0xC9E0, 0x527B8000,  1,   0, },
          { 0, 0, 0, 0x119C, 0xB844, 0x527B8000,  1,   0, },
          { 0, 0, 0, 0x119C, 0xA6A8, 0x527B8000,  1,   0, },
          { 0, 0, 0, 0x119C, 0x950C, 0x527B8000,  1,   0, },
          { 0, 0, 0, 0x119C, 0x8370, 0x527B8000,  1,   0, },
          { 0, 0, 0, 0x0F6B, 0xE3A8, 0xA4F70000,  0,   0, },
          { 0, 0, 0, 0x0F6B, 0xD43D, 0xA4F70000,  0,   0, },
          { 0, 0, 0, 0x0F6B, 0xC4D2, 0xA4F70000,  0,   0, },
          { 0, 0, 0, 0x0F6B, 0xB567, 0xA4F70000,  0,   0, },
          { 1, 0, 0, 0x0F6B, 0xA5FC, 0xA4F70000,  0,   0, },
          { 1, 0, 0, 0x1424, 0xF6B0, 0xE6696000,  4,   0, },
          { 0, 0, 0, 0x1AA9, 0xA120, 0x1EEB0000,  1,   0, },
          { 0, 0, 0, 0x1AA9, 0x8677, 0x1EEB0000,  1,   0, },
          { 0, 0, 0, 0x174E, 0xD79C, 0x3DD60000,  0,   0, },
          { 0, 0, 0, 0x174E, 0xC04E, 0x3DD60000,  0,   0, },
          { 0, 0, 0, 0x174E, 0xA900, 0x3DD60000,  0,   0, },
          { 0, 0, 0, 0x174E, 0x91B2, 0x3DD60000,  0,   0, },
          { 0, 0, 0, 0x1424, 0xF4C8, 0x7BAD3A00,  7,   0, },
          { 0, 0, 0, 0x1424, 0xE0A4, 0x7BAD3A00,  7,   0, },
          { 0, 0, 0, 0x1424, 0xCC80, 0x7BAD3A00,  7,   0, },
          { 0, 0, 0, 0x1424, 0xB85C, 0x7BAD3A00,  7,   0, },
          { 0, 0, 0, 0x1424, 0xA438, 0x7BAD3A00,  7,   0, },
          { 0, 0, 0, 0x1424, 0x9014, 0x7BAD3A00,  7,   0, },
          { 1, 0, 0, 0x119C, 0xF7E0, 0xF75A7400,  6,   0, },
          { 1, 0, 0, 0x1424, 0x8CE0, 0x88B3A000,  3,   0, },
          { 0, 0, 0, 0x1AA9, 0xA120, 0x7FBD0000,  0,   0, },
          { 1, 0, 0, 0x1AA9, 0x8677, 0x7FBD0000,  0,   0, },
          { 0, 0, 0, 0x2516, 0xD548, 0x9F7A8800,  5,   0, },
          { 1, 0, 0, 0x2516, 0xB032, 0x9F7A8800,  5,   0, },
          { 0, 0, 0, 0x299A, 0x9458, 0x517A2000,  3,   0, },
          { 0, 0, 0, 0x2516, 0xD57C, 0xA2F44000,  2,   0, },
          { 1, 0, 0, 0x2516, 0xB066, 0xA2F44000,  2,   0, },
          { 0, 0, 0, 0x299A, 0x9458, 0x5E910000,  0,   0, },
          { 1, 0, 0, 0x2516, 0xD57C, 0xBD22F000,  7,   0, },
          { 0, 0, 0, 0x299A, 0x9458, 0x32F3C000,  5,   0, },
          { 0, 0, 0, 0x2516, 0xD57C, 0x65E78000,  4,   0, },
          { 0, 0, 0, 0x2516, 0xB066, 0x65E78000,  4,   0, },
          { 0, 0, 0, 0x2516, 0x8B50, 0x65E78000,  4,   0, },
          { 1, 0, 0, 0x1EDF, 0xCC74, 0xCBCF0000,  3,   0, },
          { 1, 0, 0, 0x2516, 0xF6F8, 0xF1D00000,  0,   0, },
          { 1, 0, 0, 0x299A, 0x9458, 0x7FB95400,  6,   0, },
          { 0, 0, 0, 0x32B4, 0xA668, 0x53ED5000,  4,   0, },
          { 0, 0, 0, 0x2E17, 0xE768, 0xA7DAA000,  3,   0, },
          { 1, 0, 0, 0x2E17, 0xB951, 0xA7DAA000,  3,   0, },
          { 0, 0, 0, 0x32B4, 0xB85C, 0x72828000,  1,   0, },
          { 1, 0, 0, 0x32B4, 0x85A8, 0x72828000,  1,   0, },
          { 0, 0, 0, 0x3C3D, 0xCAD0, 0x7E3B7E00,  7,   0, },
          { 1, 0, 0, 0x3C3D, 0x8E93, 0x7E3B7E00,  7,   0, },
          { 0, 0, 0, 0x415E, 0xF0F4, 0xAF95F800,  5,   0, },
          { 1, 0, 0, 0x415E, 0xAF96, 0xAF95F800,  5,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BBF000,  4,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C71E000,  3,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BBC000,  2,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C718000,  1,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BB0000,  0,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C71FE00,  7,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BBFC00,  6,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C71F800,  5,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BBF000,  4,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C71E000,  3,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BBC000,  2,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C718000,  1,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BB0000,  0,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C71F800,  7,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BBF000,  6,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C71E000,  5,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BBC000,  4,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C718000,  3,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82BB0000,  2,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C700000,  1,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82B80000,  0,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C6AA200,  7,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82AD4400,  6,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8C548800,  5,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x82811000,  4,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8BFC2000,  3,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x81D04000,  2,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x8A9A8000,  1,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x7F0D0000,  0,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x85150800,  7,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x74021000,  6,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x6EFE2000,  5,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x47D44000,  4,   0, },
          { 0, 0, 0, 0x415E, 0x8C72, 0x16A28000,  3,   0, },
          { 0, 0, 0, 0x3C3D, 0x9628, 0x2D450000,  2,   0, },
          { 0, 0, 0, 0x375E, 0xB3D6, 0x5A8A0000,  1,   0, },
          { 0, 0, 0, 0x32B4, 0xF8F0, 0xB5140000,  0,   0, },
          { 1, 0, 0, 0x32B4, 0xC63C, 0xB5140000,  0,   0, },
          { 0, 0, 0, 0x3C3D, 0xCAD0, 0x86331C00,  6,   0, },
          { 1, 0, 0, 0x3C3D, 0x8E93, 0x86331C00,  6,   0, },
          { 1, 0, 0, 0x415E, 0xF0F4, 0xCF747000,  4,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x3FBCE000,  3,   0, },
          { 0, 0, 0, 0x415E, 0x8C72, 0x0673C000,  2,   0, },
          { 0, 0, 0, 0x3C3D, 0x9628, 0x0CE78000,  1,   0, },
          { 0, 0, 0, 0x375E, 0xB3D6, 0x19CF0000,  0,   0, },
          { 0, 0, 0, 0x32B4, 0xF8F0, 0x339F9C00,  7,   0, },
          { 0, 0, 0, 0x32B4, 0xC63C, 0x339F9C00,  7,   0, },
          { 0, 0, 0, 0x32B4, 0x9388, 0x339F9C00,  7,   0, },
          { 0, 0, 0, 0x2E17, 0xC1A8, 0x673F3800,  6,   0, },
          { 1, 0, 0, 0x2E17, 0x9391, 0x673F3800,  6,   0, },
          { 0, 0, 0, 0x32B4, 0xB85C, 0x0714E000,  4,   0, },
          { 0, 0, 0, 0x32B4, 0x85A8, 0x0714E000,  4,   0, },
          { 0, 0, 0, 0x2E17, 0xA5E8, 0x0E29C000,  3,   0, },
          { 0, 0, 0, 0x299A, 0xEFA2, 0x1C538000,  2,   0, },
          { 0, 0, 0, 0x299A, 0xC608, 0x1C538000,  2,   0, },
          { 0, 0, 0, 0x299A, 0x9C6E, 0x1C538000,  2,   0, },
          { 0, 0, 0, 0x2516, 0xE5A8, 0x38A70000,  1,   0, },
          { 0, 0, 0, 0x2516, 0xC092, 0x38A70000,  1,   0, },
          { 0, 0, 0, 0x2516, 0x9B7C, 0x38A70000,  1,   0, },
          { 0, 0, 0, 0x1EDF, 0xECCC, 0x714E0000,  0,   0, },
          { 0, 0, 0, 0x1EDF, 0xCDED, 0x714E0000,  0,   0, },
          { 0, 0, 0, 0x1EDF, 0xAF0E, 0x714E0000,  0,   0, },
          { 0, 0, 0, 0x1EDF, 0x902F, 0x714E0000,  0,   0, },
          { 1, 0, 0, 0x1AA9, 0xE2A0, 0xE29DF200,  7,   0, },
          { 1, 0, 0, 0x2516, 0xD548, 0xD5379000,  4,   0, },
          { 1, 0, 0, 0x299A, 0x9458, 0x94164000,  2,   0, },
          { 1, 0, 0, 0x32B4, 0xA668, 0xA5610000,  0,   0, },
          { 1, 0, 0, 0x3C3D, 0xCAD0, 0xC6B4E400,  6,   0, },
          { 1, 0, 0, 0x415E, 0xF0F4, 0xE0879000,  4,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x61E32000,  3,   0, },
          { 0, 0, 0, 0x415E, 0x8C72, 0x4AC04000,  2,   0, },
          { 1, 0, 0, 0x3C3D, 0x9628, 0x95808000,  1,   0, },
          { 1, 0, 0, 0x415E, 0xF0F4, 0xEE560000,  7,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x7D800000,  6,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x81FA0000,  5,   0, },
          { 0, 0, 1, 0x4639, 0x82BC, 0x6DCC0000,  4,   0, },
          { 1, 0, 0, 0x415E, 0x8C72, 0x62920000,  3,   0, },
          { 1, 0, 1, 0x4639, 0x82BC, 0x2EFC0000,  2,   0, },
          { 1, 0, 0, 0x4B85, 0xF20C, 0xBBF00000,  0,   0, },
          { 1, 0, 1, 0x504F, 0x970A, 0x2AD25000,  7,   0, },
          { 0, 0, 1, 0x5522, 0x8D76, 0x55A4A000,  6,   0, },
          { 0, 0, 0, 0x504F, 0xAA44, 0x3AA14000,  5,   0, },
          { 1, 0, 0, 0x4B85, 0xB3EA, 0x75428000,  4,   0, },
          { 1, 0, 1, 0x504F, 0x970A, 0x19BB0000,  3,   0, },
          { 1, 0, 1, 0x5522, 0x8D76, 0x33760000,  2,   0, },
          { 1, 0, 0, 0x59EB, 0xE150, 0xCDD80000,  0,   0, },
          { 0, 1, 0, 0x59EB, 0xB3D6, 0x8CE6FA00,  7,   0, },
          { 1, 0, 0, 0x59EB, 0xB3D6, 0x65F7F400,  6,   0, },
          { 1, 1, 0, 0x59EB, 0xB3D6, 0x1819E800,  5,   0, },
          { 1, 1, 0, 0x5522, 0xB3D6, 0x3033D000,  4,   0, },
          { 1, 1, 0, 0x504F, 0xBD68, 0x6067A000,  3,   0, },
          { 0, 1, 0, 0x4B85, 0xDA32, 0xC0CF4000,  2,   0, },
          { 1, 1, 1, 0x504F, 0x970A, 0x64448000,  1,   0, },
          { 1, 1, 0, 0x4B85, 0xA09E, 0x3B130000,  0,   0, },
          { 0, 1, 0, 0x4639, 0xAA32, 0x76268C00,  7,   0, },
          { 0, 1, 1, 0x4B85, 0x8C72, 0x245B1800,  6,   0, },
          { 1, 1, 1, 0x504F, 0x81DA, 0x48B63000,  5,   0, },
          { 1, 1, 0, 0x4B85, 0xA09E, 0x2E566000,  4,   0, },
          { 1, 1, 0, 0x4639, 0xAA32, 0x5CACC000,  3,   0, },
          { 0, 1, 0, 0x415E, 0xC7F2, 0xB9598000,  2,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x658B0000,  1,   0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x52100000,  0,   0, },
          { 0, 1, 1, 0x4639, 0x82BC, 0x0DF8E000,  7,   0, },
          { 1, 1, 0, 0x4B85, 0xF20C, 0x37E38000,  5,   0, },
          { 1, 1, 0, 0x4B85, 0xA687, 0x37E38000,  5,   0, },
          { 1, 1, 0, 0x4639, 0xB604, 0x6FC70000,  4,   0, },
          { 0, 1, 0, 0x415E, 0xDF96, 0xDF8E0000,  3,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x82AC0000,  2,   0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x8C520000,  1,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x827C0000,  0,   0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x8BF31C00,  7,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x81BE3800,  6,   0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x8A767000,  5,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x7EC4E000,  4,   0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x8483C000,  3,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x72DF8000,  2,   0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x6CB90000,  1,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x434A0000,  0,   0, },
          { 1, 1, 0, 0x415E, 0x8C72, 0x0D8F9600,  7,   0, },
          { 1, 1, 0, 0x3C3D, 0x9628, 0x1B1F2C00,  6,   0, },
          { 1, 1, 0, 0x375E, 0xB3D6, 0x363E5800,  5,   0, },
          { 1, 1, 0, 0x32B4, 0xF8F0, 0x6C7CB000,  4,   0, },
          { 1, 1, 0, 0x32B4, 0xC63C, 0x6C7CB000,  4,   0, },
          { 0, 1, 0, 0x32B4, 0x9388, 0x6C7CB000,  4,   0, },
          { 1, 1, 0, 0x3C3D, 0xCAD0, 0x2EA2C000,  2,   0, },
          { 1, 1, 0, 0x3C3D, 0x8E93, 0x2EA2C000,  2,   0, },
          { 1, 1, 0, 0x375E, 0xA4AC, 0x5D458000,  1,   0, },
          { 0, 1, 0, 0x32B4, 0xDA9C, 0xBA8B0000,  0,   0, },
          { 1, 1, 0, 0x3C3D, 0xCAD0, 0x4A8F0000,  6,   0, },
          { 1, 1, 0, 0x3C3D, 0x8E93, 0x4A8F0000,  6,   0, },
          { 0, 1, 0, 0x375E, 0xA4AC, 0x951E0000,  5,   0, },
          { 1, 1, 0, 0x3C3D, 0xDD78, 0x9F400000,  3,   0, },
          { 0, 1, 0, 0x3C3D, 0xA13B, 0x9F400000,  3,   0, },
          { 0, 1, 0, 0x415E, 0xF0F4, 0xE9080000,  1,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x72E40000,  0,   0, },
          { 0, 1, 0, 0x415E, 0x8C72, 0x6CC3EC00,  7,   0, },
          { 1, 1, 1, 0x4639, 0x82BC, 0x435FD800,  6,   0, },
          { 1, 1, 0, 0x415E, 0x8C72, 0x0DB9B000,  5,   0, },
          { 1, 1, 0, 0x3C3D, 0x9628, 0x1B736000,  4,   0, },
          { 1, 1, 0, 0x375E, 0xB3D6, 0x36E6C000,  3,   0, },
          { 1, 1, 0, 0x32B4, 0xF8F0, 0x6DCD8000,  2,   0, },
          { 1, 1, 0, 0x32B4, 0xC63C, 0x6DCD8000,  2,   0, },
          { 0, 1, 0, 0x32B4, 0x9388, 0x6DCD8000,  2,   0, },
          { 1, 1, 0, 0x3C3D, 0xCAD0, 0x33E60000,  0,   0, },
          { 1, 1, 0, 0x3C3D, 0x8E93, 0x33E60000,  0,   0, },
          { 1, 1, 0, 0x375E, 0xA4AC, 0x67CC0000,  7,   0, },
          { 0, 1, 0, 0x32B4, 0xDA9C, 0xCF980000,  6,   0, },
          { 0, 1, 0, 0x3C3D, 0xCAD0, 0x9EC00000,  4,   0, },
          { 1, 1, 0, 0x415E, 0xF0F4, 0x40B40000,  2,   0, },
          { 1, 1, 0, 0x415E, 0xAF96, 0x40B40000,  2,   0, },
          { 1, 1, 0, 0x3C3D, 0xDC70, 0x81680000,  1,   0, },
          { 0, 1, 0, 0x3C3D, 0xA033, 0x81680000,  1,   0, },
          { 1, 1, 0, 0x415E, 0xF0F4, 0x75C80000,  7,   0, },
          { 0, 1, 0, 0x415E, 0xAF96, 0x75C80000,  7,   0, },
          { 0, 1, 1, 0x4639, 0x82BC, 0x0F200000,  6,   0, },
          { 1, 1, 0, 0x4B85, 0xF20C, 0x3C800000,  4,   0, },
          { 1, 1, 0, 0x4B85, 0xA687, 0x3C800000,  4,   0, },
          { 0, 1, 0, 0x4639, 0xB604, 0x79000000,  3,   0, },
          { 0, 1, 1, 0x4B85, 0x8C72, 0x126A0000,  2,   0, },
        } };

        void checkDecoder(const xArithCoreModel& Model, const xArithCoreDec& Dec, const tTraceEntry& TestSet)
        {
            // Retrieving current values ​​from the decoder and model
            uint32_t TstProbIdx = Model.getProbIndex();
            uint32_t TstMPS = Model.getMPS();
            uint32_t TstQe = Model.getQe(); // Using getQe() for consistency
            uint32_t TstC = Dec.getC();
            uint32_t TstA = Dec.getA();
            uint32_t TstCT = Dec.getCT();

            // Getting expected values ​​from the TestSet structure
            uint32_t D = TestSet.D;
            uint32_t MPS = TestSet.MPS;
            uint32_t Qe = TestSet.Qe;
            uint32_t A = TestSet.A;
            uint32_t C = TestSet.C;
            uint32_t CT = TestSet.CT;

            // Validation of the A value
            bool CorrectA = TstA == 0 || TstA > 0x7FFF;
            CHECK(CorrectA);

            // Comparison with expected values ​​(if not UNDEFINED)
            if (MPS != UND) { CHECK(TstMPS == MPS); }
            if (Qe != UND) { CHECK(TstQe == Qe); }
            if (A != UND) { CHECK(TstA == A); }
            if (C != UND) { CHECK(TstC == C); }
            if (CT != UND) { CHECK(TstCT == CT); }
        }

        void testDec()
        {
            xByteBuffer     Buff(4096);
            xArithCoreModel Model;
            xArithCoreDec   Dec;
            Model.init();
            Buff.appendBytes(EncodedBytes.data(), EncodedBytes.size());
            Dec.setByteBuffer(&Buff);

            Dec.Initdec();

            for (size_t i = 0; i < DecoderTrace.size(); i++)
            {
                checkDecoder(Model, Dec, DecoderTrace[i]);
                const uint32 RefBin = DecoderTrace[i].D;
                uint32 DecBin = Dec.Decode(Model);
                CHECK(RefBin == DecBin);
            }
        }

        //===============================================================================================================================================================================================================

        void testEncDec1M()
        {
            xByteBuffer     Buff(64 * c_NumUnits + 1024);
            xArithCoreModel Model;
            xBitstreamWriter Writer;
            Writer.bindByteBuffer(&Buff);
            xArithCoreEnc Enc(Writer);
            xArithCoreDec   Dec;

            Enc.setByteBuffer(&Buff);
            Dec.setByteBuffer(&Buff);

            uint32 State = xTestUtils::c_XorShiftSeed;
            Model.init();
            Enc.initialize(); // instead of start()
            for (int32 i = 0; i < c_NumUnits; i++)
            {
                State = xTestUtils::xXorShift32(State);

                for (int32 j = 0; j < 32; j++)
                {
                    uint32 Data = (State >> j) & 0x1;
                    Enc.encodeBinMP(Data, Model);
                }
            }
            for (int32 i = 0; i < 256; i++)
            {
                Enc.encodeBinMP(0, Model);
            }
            for (int32 i = 0; i < 256; i++)
            {
                Enc.encodeBinMP(1, Model);
            }
            Enc.finish();

            State = xTestUtils::c_XorShiftSeed;
            Model.init();
            Dec.Initdec();
            for (int32 i = 0; i < c_NumUnits; i++)
            {
                State = xTestUtils::xXorShift32(State);

                uint32 Data = 0;
                for (int32 j = 0; j < 32; j++)
                {
                    Data |= (Dec.Decode(Model) << j);
                }
                CHECK(Data == State);
            }
            for (int32 i = 0; i < 256; i++)
            {
                int32 D = Dec.Decode(Model);
                CHECK(D == 0);
            }
            for (int32 i = 0; i < 256; i++)
            {
                int32 D = Dec.Decode(Model);
                CHECK(D == 1);
            }
            Dec.finish();
        }

        //===============================================================================================================================================================================================================
TEST_CASE("testEnc")
{
    testEnc();
}

TEST_CASE("Carry-over handling when flushing the buffer with real writer") {
    using namespace PMBB;
    using namespace PMBB::JPEG;

    // Klasa pomocnicza z metodą do ustawiania stanu
    class TestableArithCoreEnc : public xArithCoreEncT<xBitstreamWriter> {
    public:
        using xArithCoreEncT<xBitstreamWriter>::xArithCoreEncT;
        void setInternalStateFull(uint32_t A, uint32_t C, int CT,
            uint8_t B, uint32_t ST, bool isFirst)
        {
            m_A = A;
            m_C = C;
            m_CT = CT;
            m_B = B;
            m_ST = ST;
            m_isFirstByte = isFirst;
        }
    };

    // 1. Bufor i writer
    xByteBuffer output_buffer(1024);
    xBitstreamWriter writer;
    writer.bindByteBuffer(&output_buffer);

    // 2. Encoder
    TestableArithCoreEnc enc(writer);
    enc.initialize();

    // 3. Ustaw stan wewnętrzny
    enc.setInternalStateFull(0x8001, 0xFFFF, 1, /*B*/0x01, /*ST*/2, /*isFirst*/false);

    // 4. Flush
    enc.finish();

    // 5. Pobierz bajty
    std::vector<uint8_t> actualBytes(
        output_buffer.getReadPtr(),
        output_buffer.getReadPtr() + output_buffer.getDataSize()
    );

    // 6. Oczekiwane
    std::vector<uint8_t> expectedBytes = { 0x01, 0xff, 0x00, 0xff, 0x00, 0x00 };

    // 7. Sprawdzenie
    CHECK(actualBytes == expectedBytes);
}

TEST_CASE("Arithmetic encoder generates expected EncodedBytes") {
    using namespace PMBB;
    using namespace PMBB::JPEG;

    // 1. Bufor i writer
    xByteBuffer Buff(4096);
    xBitstreamWriter Writer;
    Writer.bindByteBuffer(&Buff);

    // 2. Model i Encoder
    xArithCoreModel Model;
    Model.init();

    xArithCoreEnc Enc(Writer);
    Enc.initialize();

    // 3. Zakodowanie wszystkich symboli z EncoderTrace
    for (size_t i = 0; i < EncoderTrace.size() - 1; i++) {
        checkEncoder(Model, Enc, EncoderTrace[i]);  // sprawdzenie stanu przed kodowaniem
        uint32 D = EncoderTrace[i].D;               // symbol do zakodowania
        Enc.encodeBinMP(D, Model);                  // kodowanie
    }

    // 4. Finalna weryfikacja stanu przed finish()
    checkEncoder(Model, Enc, EncoderTrace.back());

    // 5. Flush encodera
    Enc.finish();

    // 6. Sprawdzenie zgodności bajtów z EncodedBytes
    static const std::array<uint8_t, 29> EncodedBytes = {
        0x65, 0x5B, 0x51, 0x44, 0xF7, 0x96, 0x9D, 0x51,
        0x78, 0x55, 0xBF, 0xFF, 0x00, 0xFC, 0x51, 0x84,
        0xC7, 0xCE, 0xF9, 0x39, 0x00, 0x28, 0x7D, 0x46,
        0x70, 0x8E, 0xCB, 0xC0, 0xF6
    };

    uint32 NumBytesWritten = Buff.getDataSize();
    REQUIRE(NumBytesWritten == EncodedBytes.size());  // wymaga tej samej liczby bajtów

    for (size_t i = 0; i < EncodedBytes.size(); i++) {
        uint8_t EncByte = Buff.getReadPtr()[i];
        uint8_t RefByte = EncodedBytes[i];
        CHECK(EncByte == RefByte);  // sprawdzenie dokładnej wartości
    }

    std::cout << ">>> Encoder test completed successfully <<<\n";
}

TEST_CASE("Arithmetic encoder generates expected EncodedBytes with detailed diff") {
    using namespace PMBB;
    using namespace PMBB::JPEG;

    // 1. Bufor i writer
    xByteBuffer Buff(4096);
    xBitstreamWriter Writer;
    Writer.bindByteBuffer(&Buff);

    // 2. Model i Encoder
    xArithCoreModel Model;
    Model.init();

    xArithCoreEnc Enc(Writer);
    Enc.initialize();

    // 3. Kodowanie wszystkich symboli z EncoderTrace
    for (size_t i = 0; i < EncoderTrace.size() - 1; i++) {
        checkEncoder(Model, Enc, EncoderTrace[i]);  // sprawdzenie stanu przed kodowaniem
        uint32 D = EncoderTrace[i].D;               // symbol do zakodowania
        Enc.encodeBinMP(D, Model);                  // kodowanie
    }

    // 4. Finalna weryfikacja stanu przed finish()
    checkEncoder(Model, Enc, EncoderTrace.back());

    // 5. Flush encodera
    Enc.finish();

    // 6. Sprawdzenie zgodności bajtów z EncodedBytes
    static const std::array<uint8_t, 29> EncodedBytes = {
        0x65, 0x5B, 0x51, 0x44, 0xF7, 0x96, 0x9D, 0x51,
        0x78, 0x55, 0xBF, 0xFF, 0x00, 0xFC, 0x51, 0x84,
        0xC7, 0xCE, 0xF9, 0x39, 0x00, 0x28, 0x7D, 0x46,
        0x70, 0x8E, 0xCB, 0xC0, 0xF6
    };

    uint32 NumBytesWritten = Buff.getDataSize();
    REQUIRE(NumBytesWritten == EncodedBytes.size());  // wymaga tej samej liczby bajtów

    bool allMatch = true;

    for (size_t i = 0; i < EncodedBytes.size(); i++) {
        uint8_t EncByte = Buff.getReadPtr()[i];
        uint8_t RefByte = EncodedBytes[i];
        if (EncByte != RefByte) {
            allMatch = false;
            std::cerr << "Mismatch at byte " << i
                << ": Encoded=0x" << std::hex << int(EncByte)
                << " Expected=0x" << std::hex << int(RefByte) << std::dec << "\n";
        }
        CHECK(EncByte == RefByte);
    }

    if (allMatch) {
        std::cout << ">>> Encoder test completed successfully <<<\n";
    }
    else {
        std::cerr << ">>> Encoder test failed: some bytes do not match <<<\n";
    }
}

TEST_CASE("testDec")
{
    testDec();
}
