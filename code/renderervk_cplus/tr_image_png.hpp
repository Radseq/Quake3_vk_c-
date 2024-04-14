#ifndef TR_IMAGE_PNG_HPP
#define TR_IMAGE_PNG_HPP



#ifdef __cplusplus
extern "C"
{
#endif

#include "q_shared.hpp"
#include "../renderervk/tr_public.h"
#include "q_platform.hpp"
#include "puff.hpp"

// we could limit the png size to a lower value here
#ifndef INT_MAX
#define INT_MAX 0x1fffffff
#endif

    /*
    =================
    PNG LOADING
    =================
    */

    /*
     *  Quake 3 image format : RGBA
     */

#define Q3IMAGE_BYTESPERPIXEL (4)

    /*
     *  PNG specifications
     */

    /*
     *  The first 8 Bytes of every PNG-File are a fixed signature
     *  to identify the file as a PNG.
     */

#define PNG_Signature "\x89\x50\x4E\x47\xD\xA\x1A\xA"
#define PNG_Signature_Size (8)

    /*
     *  After the signature diverse chunks follow.
     *  A chunk consists of a header and if Length
     *  is bigger than 0 a body and a CRC of the body follow.
     */

    struct PNG_ChunkHeader
    {
        uint32_t Length;
        uint32_t Type;
    };

#define PNG_ChunkHeader_Size (8)

    typedef uint32_t PNG_ChunkCRC;

#define PNG_ChunkCRC_Size (4)

    /*
     *  We use the following ChunkTypes.
     *  All others are ignored.
     */

#define MAKE_CHUNKTYPE(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d)))

#define PNG_ChunkType_IHDR MAKE_CHUNKTYPE('I', 'H', 'D', 'R')
#define PNG_ChunkType_PLTE MAKE_CHUNKTYPE('P', 'L', 'T', 'E')
#define PNG_ChunkType_IDAT MAKE_CHUNKTYPE('I', 'D', 'A', 'T')
#define PNG_ChunkType_IEND MAKE_CHUNKTYPE('I', 'E', 'N', 'D')
#define PNG_ChunkType_tRNS MAKE_CHUNKTYPE('t', 'R', 'N', 'S')

    /*
     *  Per specification the first chunk after the signature SHALL be IHDR.
     */

    struct PNG_Chunk_IHDR
    {
        uint32_t Width;
        uint32_t Height;
        uint8_t BitDepth;
        uint8_t ColourType;
        uint8_t CompressionMethod;
        uint8_t FilterMethod;
        uint8_t InterlaceMethod;
    };

#define PNG_Chunk_IHDR_Size (13)

    /*
     *  ColourTypes
     */

#define PNG_ColourType_Grey (0)
#define PNG_ColourType_True (2)
#define PNG_ColourType_Indexed (3)
#define PNG_ColourType_GreyAlpha (4)
#define PNG_ColourType_TrueAlpha (6)

    /*
     *  number of colour components
     *
     *  Grey      : 1 grey
     *  True      : 1 R, 1 G, 1 B
     *  Indexed   : 1 index
     *  GreyAlpha : 1 grey, 1 alpha
     *  TrueAlpha : 1 R, 1 G, 1 B, 1 alpha
     */

#define PNG_NumColourComponents_Grey (1)
#define PNG_NumColourComponents_True (3)
#define PNG_NumColourComponents_Indexed (1)
#define PNG_NumColourComponents_GreyAlpha (2)
#define PNG_NumColourComponents_TrueAlpha (4)

    /*
     *  For the different ColourTypes
     *  different BitDepths are specified.
     */

#define PNG_BitDepth_1 (1)
#define PNG_BitDepth_2 (2)
#define PNG_BitDepth_4 (4)
#define PNG_BitDepth_8 (8)
#define PNG_BitDepth_16 (16)

    /*
     *  Only one valid CompressionMethod is standardized.
     */

#define PNG_CompressionMethod_0 (0)

    /*
     *  Only one valid FilterMethod is currently standardized.
     */

#define PNG_FilterMethod_0 (0)

    /*
     *  This FilterMethod defines 5 FilterTypes
     */

#define PNG_FilterType_None (0)
#define PNG_FilterType_Sub (1)
#define PNG_FilterType_Up (2)
#define PNG_FilterType_Average (3)
#define PNG_FilterType_Paeth (4)

    /*
     *  Two InterlaceMethods are standardized :
     *  0 - NonInterlaced
     *  1 - Interlaced
     */

#define PNG_InterlaceMethod_NonInterlaced (0)
#define PNG_InterlaceMethod_Interlaced (1)

    /*
     *  The Adam7 interlace method uses 7 passes.
     */

#define PNG_Adam7_NumPasses (7)

    /*
     *  The compressed data starts with a header ...
     */

    struct PNG_ZlibHeader
    {
        uint8_t CompressionMethod;
        uint8_t Flags;
    };

#define PNG_ZlibHeader_Size (2)

    /*
     *  ... and is followed by a check value
     */

#define PNG_ZlibCheckValue_Size (4)

    /*
     *  Some support functions for buffered files follow.
     */

    /*
     *  buffered file representation
     */

    struct BufferedFile
    {
        byte *Buffer;
        int Length;
        byte *Ptr;
        int BytesLeft;
    };

    void R_LoadPNG_plus(const char *name, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_PNG_HPP
