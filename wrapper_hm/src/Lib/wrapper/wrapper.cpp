#include<iostream>
#include<fstream>
using namespace std;

#include "wrapper.h"
#include "TLibDecoder/TDecTop.h"
#include "TLibDecoder/AnnexBread.h"
#include "TLibDecoder/NALread.h"
#include "TLibCommon/NAL.h"
#include "TLibCommon/TComBitStream.h"

#ifdef WIN32
#pragma comment (linker, "/export:_libDecoderInit")
#pragma comment (linker, "/export:_libDecoderDecode")
#pragma comment (linker, "/export:_libDecoderClose")
#pragma comment (linker, "/export:_libDecoderGetPictureSize")
#pragma comment (linker, "/export:_libDecoderGetOuptut")
#pragma comment (linker, "/export:_libDecoderVersion")
#endif

TDecTop             *myDecoder;
bool                g_md5_mismatch= false;
Int                 m_iPOCLastDisplay;                    ///< last POC in display order
Int                 m_iSkipFrame;
UInt                m_outputBitDepth =0u;                     ///< bit depth used for writing output

Int                 m_iMaxTemporalLayer = -1;                  ///< maximum temporal layer to be decoded
Int                 m_decodedPictureHashSEIEnabled = 0;               ///< Checksum(3)/CRC(2)/MD5(1)/disable(0) acting on SEI picture_digest

Int                poc;
TComList<TComPic*>* pcListPic = NULL;


int libDecoderInit()
{
    myDecoder= new TDecTop;
    myDecoder->create();
    myDecoder->init();
    m_iPOCLastDisplay  = -MAX_INT;
    m_iSkipFrame = 0;
    m_iPOCLastDisplay += m_iSkipFrame;      // set the last displayed POC correctly for skip forward.
    myDecoder->setDecodedPictureHashSEIEnabled(m_decodedPictureHashSEIEnabled);
    return 0;
}

static bool saveComponent(unsigned char *buf, Pel* src, UInt src_stride, UInt width, UInt height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            buf[x] = ((unsigned short) src[x] ) & 0xff;
        }
        
        src += src_stride;
        buf += width;
    }
    return true;
}

/** \param pcListPic list of pictures to be written to file
 \todo            DYN_REF_FREE should be revised
 */
Void xFlushOutput( TComList<TComPic*>* pcListPic)
{
    if(!pcListPic)
    {
        return;
    }
    TComList<TComPic*>::iterator iterPic   = pcListPic->begin();
    
    iterPic   = pcListPic->begin();
    
    while (iterPic != pcListPic->end())
    {
        TComPic* pcPic = *(iterPic);
        
        if ( pcPic->getOutputMark() )
        {
            // update POC of display order
            m_iPOCLastDisplay = pcPic->getPOC();
            
            // erase non-referenced picture in the reference picture list after display
            if ( !pcPic->getSlice(0)->isReferenced() && pcPic->getReconMark() == true )
            {
#if !DYN_REF_FREE
                pcPic->setReconMark(false);
                
                // mark it should be extended later
                pcPic->getPicYuvRec()->setBorderExtension( false );
                
#else
                pcPic->destroy();
                pcListPic->erase( iterPic );
                iterPic = pcListPic->begin(); // to the beginning, non-efficient way, have to be revised!
                continue;
#endif
            }
            pcPic->setOutputMark(false);
        }
#if !DYN_REF_FREE
        if(pcPic)
        {
            pcPic->destroy();
            delete pcPic;
            pcPic = NULL;
        }
#endif    
        iterPic++;
    }
    pcListPic->clear();
    m_iPOCLastDisplay = -MAX_INT;
}

/** \param pcListPic list of pictures to be written to file
 \todo            DYN_REF_FREE should be revised
 */
Int xWriteOutput( TComList<TComPic*>* pcListPic, UInt tId, unsigned char * Y, unsigned char *U, unsigned char *V, int force_flush)
{
    TComList<TComPic*>::iterator iterPic   = pcListPic->begin();
    Int not_displayed = 0;
	Int written = 0;
    
    while (iterPic != pcListPic->end())
    {
        TComPic* pcPic = *(iterPic);
        if(pcPic->getOutputMark() && pcPic->getPOC() > m_iPOCLastDisplay)
        {
            not_displayed++;
        }
        iterPic++;
    }
    iterPic   = pcListPic->begin();
    
    while (iterPic != pcListPic->end())
    {
        TComPic* pcPic = *(iterPic);
        
        if ( pcPic->getOutputMark() 
			&& (force_flush || (not_displayed >  pcPic->getSlice(0)->getSPS()->getNumReorderPics(tId) )
			&& pcPic->getPOC() > m_iPOCLastDisplay))
        {
			Int cropLeft, cropRight, cropTop, cropBottom;
            // write to file
            not_displayed--;            

	        Window &crop = pcPic->getDefDisplayWindow();
			cropLeft = crop.getWindowLeftOffset();
			cropRight = crop.getWindowRightOffset();
			cropTop = crop.getWindowTopOffset();
			cropBottom = crop.getWindowBottomOffset();

			UInt yStride = pcPic->getPicYuvRec()->getStride();
			UInt uvStride = pcPic->getPicYuvRec()->getCStride();
            UInt picWidth = pcPic->getPicYuvRec()->getWidth() - cropLeft - cropRight;
            UInt picHeight = pcPic->getPicYuvRec()->getHeight() - cropTop  - cropBottom;
            
            saveComponent(Y, pcPic->getPicYuvRec()->getLumaAddr(), yStride, picWidth, picHeight);
			saveComponent(U, pcPic->getPicYuvRec()->getCbAddr(), uvStride, picWidth/2, picHeight/2);
            saveComponent(V, pcPic->getPicYuvRec()->getCrAddr(), uvStride, picWidth/2, picHeight/2);

			written ++;

			// update POC of display order
            m_iPOCLastDisplay = pcPic->getPOC();
            
            // erase non-referenced picture in the reference picture list after display
            if ( !pcPic->getSlice(0)->isReferenced() && pcPic->getReconMark() == true )
            {
#if !DYN_REF_FREE
                pcPic->setReconMark(false);
                
                // mark it should be extended later
                pcPic->getPicYuvRec()->setBorderExtension( false );
                
#else
                pcPic->destroy();
                pcListPic->erase( iterPic );
                iterPic = pcListPic->begin(); // to the beginning, non-efficient way, have to be revised!
                continue;
#endif
            }
            pcPic->setOutputMark(false);
        }
        
        iterPic++;
		if (written) 
			break;
    }
	return written;
}

int libDecoderDecode(unsigned char *buff, int len, unsigned int *temporal_id)
{
    bool bNewPicture = false;
    vector<uint8_t> nalUnit;
    InputNALUnit nalu;
	int got_picture = 0;

    int i;
    for (i=0; i < len ; i++){
        nalUnit.push_back(buff[i]);
    }
    read(nalu, nalUnit);
    //readNAL(nalu, nalUnit);
    bNewPicture=myDecoder->decode(nalu, m_iSkipFrame, m_iPOCLastDisplay);
    if (bNewPicture){
		myDecoder->executeLoopFilters(poc, pcListPic);
        got_picture=1;
    }
    if( pcListPic )
    {
        if ( bNewPicture &&
            (   nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR
				|| nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP
				|| nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA_N_LP
				|| nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLANT
				|| nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA )
		) {
            xFlushOutput( pcListPic);
        }
    }
	
	*temporal_id = nalu.m_temporalId;
	return got_picture;
}

void libDecoderGetPictureSize(unsigned int *width, unsigned int *height)
{
	TComList<TComPic*>::iterator iterPic   = pcListPic->begin();    
	iterPic   = pcListPic->begin();
	TComPic* pcPic = *(iterPic);
	*width = pcPic->getPicYuvRec()->getWidth();
	*height = pcPic->getPicYuvRec()->getHeight();

	

}

int libDecoderGetOuptut(unsigned int temporal_id, unsigned char *Y, unsigned char *U, unsigned char *V, int force_flush)
{
	return xWriteOutput( pcListPic, temporal_id, Y, U, V, force_flush);
}

void libDecoderClose( void )
{
    myDecoder->destroy();
    return;
}

const char *libDecoderVersion()
{
	return "HEVC HM v"NV_VERSION;
}
