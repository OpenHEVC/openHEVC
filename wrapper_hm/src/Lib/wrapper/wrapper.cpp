#include<iostream>
#include<fstream>
using namespace std;

#include "wrapper.h"
#include "TLibDecoder/TDecTop.h"
#include "TLibDecoder/AnnexBread.h"
#include "TLibDecoder/NALread.h"
#include "TLibCommon/NAL.h"
#include "TLibCommon/TComBitStream.h"




TDecTop             myDecoder;
bool                g_md5_mismatch= false;
Int                 m_iPOCLastDisplay;                    ///< last POC in display order
Int                 m_iSkipFrame;
UInt                m_outputBitDepth =0u;                     ///< bit depth used for writing output

Int                 m_iMaxTemporalLayer = -1;                  ///< maximum temporal layer to be decoded
Int                 m_pictureDigestEnabled = 0;               ///< Checksum(3)/CRC(2)/MD5(1)/disable(0) acting on SEI picture_digest

UInt                uiPOC;
TComList<TComPic*>* pcListPic = NULL;



int libDecoderInit( void )
{
    myDecoder.create();
    myDecoder.init();
    m_iPOCLastDisplay  = -MAX_INT;
    m_iSkipFrame = 0;
    m_iPOCLastDisplay += m_iSkipFrame;      // set the last displayed POC correctly for skip forward.
    myDecoder.setPictureDigestEnabled(m_pictureDigestEnabled);
    return 0;
}

static bool saveComponent(unsigned char *buf, Pel* src, UInt width, UInt height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            buf[x] = src[x] & 0xff;
        }
        
        src += width;
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
Void xWriteOutput( TComList<TComPic*>* pcListPic, UInt tId, unsigned char * Y, unsigned char *U, unsigned char *V )
{
    TComList<TComPic*>::iterator iterPic   = pcListPic->begin();
    Int not_displayed = 0;
    
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
        
        if ( pcPic->getOutputMark() && (not_displayed >  pcPic->getSlice(0)->getSPS()->getNumReorderPics(tId) && pcPic->getPOC() > m_iPOCLastDisplay))
        {
            // write to file
            not_displayed--;            

            UInt picWidth = pcPic->getPicYuvRec()->getWidth();
            UInt picHeight = pcPic->getPicYuvRec()->getHeight();
            
            saveComponent(Y, pcPic->getPicYuvRec()->getLumaAddr(), picWidth, picHeight);
            saveComponent(U, pcPic->getPicYuvRec()->getCbAddr(), picWidth/2, picHeight/2);
            saveComponent(V, pcPic->getPicYuvRec()->getCrAddr(), picWidth/2, picHeight/2);
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
    }
}



int libDecoderDecode(unsigned char *buff, int len, unsigned char *Y, unsigned char *U, unsigned char *V, int *got_picture)
{
    bool dontRead=false;
    bool bNewPicture = false;
    vector<uint8_t> nalUnit;
    InputNALUnit nalu;
    int i;

    for (i=0; i < len ; i++){
        nalUnit.push_back(buff[i]);
    }
    read(nalu, nalUnit);
    //readNAL(nalu, nalUnit);
    bNewPicture=myDecoder.decode(nalu, m_iSkipFrame, m_iPOCLastDisplay);
    if (bNewPicture){
        myDecoder.executeDeblockAndAlf(uiPOC, pcListPic, m_iSkipFrame, m_iPOCLastDisplay);
        dontRead=true;
    }
    if( pcListPic )
    {
        if ( bNewPicture &&
            (   nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR
             || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLANT
             || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA ) )
        {
            xFlushOutput( pcListPic);
        }
    }
    if (bNewPicture){
        xWriteOutput( pcListPic, nalu.m_temporalId, Y, U, V );
    }

    return dontRead;
}

void libDecoderClose( void )
{
    myDecoder.destroy();
    return;
}