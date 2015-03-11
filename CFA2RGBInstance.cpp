// ****************************************************************************
// PixInsight Class Library - PCL 02.00.13.0689
// Standard CFA2RGB Process Module Version 01.04.03.0144
// ****************************************************************************
// CFA2RGBInstance.cpp - Released 2014/10/29 07:35:26 UTC
// ****************************************************************************
// This file is part of the standard CFA2RGB PixInsight module.
//
// Copyright (c) 2003-2014, Pleiades Astrophoto S.L. All Rights Reserved.
//
// Redistribution and use in both source and binary forms, with or without
// modification, is permitted provided that the following conditions are met:
//
// 1. All redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. All redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. Neither the names "PixInsight" and "Pleiades Astrophoto", nor the names
//    of their contributors, may be used to endorse or promote products derived
//    from this software without specific prior written permission. For written
//    permission, please contact info@pixinsight.com.
//
// 4. All products derived from this software, in any form whatsoever, must
//    reproduce the following acknowledgment in the end-user documentation
//    and/or other materials provided with the product:
//
//    "This product is based on software from the PixInsight project, developed
//    by Pleiades Astrophoto and its contributors (http://pixinsight.com/)."
//
//    Alternatively, if that is where third-party acknowledgments normally
//    appear, this acknowledgment must be reproduced in the product itself.
//
// THIS SOFTWARE IS PROVIDED BY PLEIADES ASTROPHOTO AND ITS CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PLEIADES ASTROPHOTO OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, BUSINESS
// INTERRUPTION; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; AND LOSS OF USE,
// DATA OR PROFITS) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// ****************************************************************************

#include "CFA2RGBInstance.h"
#include "CFA2RGBParameters.h"

#include <pcl/ATrousWaveletTransform.h>
#include <pcl/AutoViewLock.h>
#include <pcl/Console.h>
#include <pcl/MetaModule.h>
#include <pcl/MuteStatus.h>
#include <pcl/SpinStatus.h>
#include <pcl/StdStatus.h>
#include <pcl/Thread.h>
#include <pcl/Version.h>

#define SRC_CHANNEL(c) (m_source.IsColor() ? c : 0)

namespace pcl
{

// ----------------------------------------------------------------------------

static IsoString ValidFullId( const IsoString& id )
{
   IsoString validId( id );
   validId.ReplaceString( "->", "_" );
   return validId;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFA2RGBInstance::CFA2RGBInstance( const MetaProcess* m ) :
ProcessImplementation( m ),
p_bayerPattern( CFA2RGBBayerPatternParameter::Default ),
o_imageId()
{
}

CFA2RGBInstance::CFA2RGBInstance( const CFA2RGBInstance& x ) :
ProcessImplementation( x )
{
   Assign( x );
}

void CFA2RGBInstance::Assign( const ProcessImplementation& p )
{
   const CFA2RGBInstance* x = dynamic_cast<const CFA2RGBInstance*>( &p );
   if ( x != 0 )
   {
      p_bayerPattern             = x->p_bayerPattern;
      o_imageId                  = x->o_imageId;
   }
}

bool CFA2RGBInstance::CanExecuteOn( const View& view, String& whyNot ) const
{
   if ( view.Image().IsComplexSample() )
      whyNot = "CFA2RGB cannot be executed on complex images.";
   else
   {
      whyNot.Clear();
      return true;
   }
   return false;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

class CFA2RGBEngine
{
public:

   template <class P>
   static void SuperPixelThreaded( Image& target, const GenericImage<P>& source, const CFA2RGBInstance& instance )
   {
      int target_w = source.Width() >> 1;
      int target_h = source.Height() >> 1;

      target.AllocateData( target_w, target_h, 3, ColorSpace::RGB );

      target.Status().Initialize( "SuperPixel CFA2RGBing", target_h );

      int numberOfThreads = Thread::NumberOfThreads( target_h, 1 );
      int rowsPerThread = target_h/numberOfThreads;

      AbstractImage::ThreadData data( target, target_h );

      PArray<SuperPixelThread<P> > threads;
      for ( int i = 0, j = 1; i < numberOfThreads; ++i, ++j )
         threads.Add( new SuperPixelThread<P>( data, target, source, instance,
                                               i*rowsPerThread,
                                               (j < numberOfThreads) ? j*rowsPerThread : target_h ) );

      AbstractImage::RunThreads( threads, data );
      threads.Destroy();

      target.Status() = data.status;
   }

private:

   template <class P>
   class CFA2RGBThreadBase : public Thread
   {
   public:

      CFA2RGBThreadBase( const AbstractImage::ThreadData& data,
                         Image& target, const GenericImage<P>& source,
                         const CFA2RGBInstance& instance, int start, int end ) :
      Thread(), m_data( data ), m_target( target ), m_source( source ), m_instance( instance ), m_start( start ), m_end( end )
      {
      }

   protected:

      const AbstractImage::ThreadData& m_data;
            Image&                     m_target;
      const GenericImage<P>&           m_source;
      const CFA2RGBInstance&           m_instance;
            int                        m_start, m_end;
   };

#define m_target   this->m_target
#define m_source   this->m_source
#define m_instance this->m_instance
#define m_start    this->m_start
#define m_end      this->m_end

   template <class P>
   class SuperPixelThread : public CFA2RGBThreadBase<P>
   {
   public:

      SuperPixelThread( const AbstractImage::ThreadData& data,
                        Image& target, const GenericImage<P>& source, const CFA2RGBInstance& instance, int start, int end ) :
      CFA2RGBThreadBase<P>( data, target, source, instance, start, end )
      {
      }

      virtual void Run()
      {
         INIT_THREAD_MONITOR()

         const int src_w2 = m_source.Width() >> 1;

         for ( int row = m_start; row < m_end; row++ )
         {
            for ( int col = 0; col < src_w2; col++ )
            {
               int red_col, red_row, green_col1, green_col2, green_row1, green_row2, blue_row, blue_col;
               switch( m_instance.p_bayerPattern )
               {
               default:
               case CFA2RGBBayerPatternParameter::RGGB:
                  red_col = (col * 2);
                  red_row = (row * 2);
                  green_col1 = (col * 2) + 1;
                  green_row1 = row * 2;
                  green_col2 = col * 2;
                  green_row2 = (row * 2) + 1;
                  blue_col = col * 2 + 1;
                  blue_row = row * 2 + 1;
                  break;
               case CFA2RGBBayerPatternParameter::BGGR:
                  red_col = (col * 2) + 1;
                  red_row = (row * 2) + 1;
                  green_col1 = (col * 2) + 1;
                  green_row1 = row * 2;
                  green_col2 = col * 2;
                  green_row2 = (row * 2) + 1;
                  blue_col = col * 2;
                  blue_row = row * 2;
                  break;
               case CFA2RGBBayerPatternParameter::GBRG:
                  red_col = (col * 2);
                  red_row = (row * 2) + 1;
                  green_col1 = (col * 2);
                  green_row1 = row * 2;
                  green_col2 = (col * 2) + 1;
                  green_row2 = (row * 2) + 1;
                  blue_col = (col * 2) + 1;
                  blue_row = row * 2;
                  break;
               case CFA2RGBBayerPatternParameter::GRBG:
                  red_col = (col * 2) + 1;
                  red_row = (row * 2);
                  green_col1 = (col * 2);
                  green_row1 = (row * 2);
                  green_col2 = (col * 2) + 1;
                  green_row2 = (row * 2) + 1;
                  blue_col = (col * 2);
                  blue_row = (row * 2) + 1;
                  break;
               }

               // red
               P::FromSample( m_target.Pixel( col, row, 0 ), m_source.Pixel( red_col, red_row, SRC_CHANNEL( 0 ) ) );
               //green
               double v1, v2;
               P::FromSample( v1, m_source.Pixel( green_col1, green_row1, SRC_CHANNEL( 1 ) ) );
               P::FromSample( v2, m_source.Pixel( green_col2, green_row2, SRC_CHANNEL( 1 ) ) );
               m_target.Pixel( col, row, 1 ) = (v1 + v2)/2;
               // blue
               P::FromSample( m_target.Pixel( col, row, 2 ), m_source.Pixel( blue_col, blue_row, SRC_CHANNEL( 2 ) ) );
            }

            UPDATE_THREAD_MONITOR( 16 )
         }
      }
   }; // SuperPixelThread

#undef m_target
#undef m_source
#undef m_instance
#undef m_start
#undef m_end

}; // CFA2RGBEngine

// ----------------------------------------------------------------------------

bool CFA2RGBInstance::ExecuteOn( View& view )
{
   o_imageId.Clear();

   AutoViewLock lock( view );

   ImageVariant source = view.Image();
   if ( source.IsComplexSample() )
      return false;

   source.SetColorSpace(ColorSpace::RGB);

   IsoString patternId;
   switch ( p_bayerPattern )
   {
   case CFA2RGBBayerPatternParameter::RGGB: patternId = "RGGB"; break;
   case CFA2RGBBayerPatternParameter::BGGR: patternId = "BGGR"; break;
   case CFA2RGBBayerPatternParameter::GBRG: patternId = "GBRG"; break;
   case CFA2RGBBayerPatternParameter::GRBG: patternId = "GRBG"; break;
   default:
      throw Error( "Internal error: Invalid Bayer pattern!" );
   }
   /*
   IsoString baseId = ValidFullId( view.FullId() ) + "_b";

   ImageWindow targetWindow(  1,    // width
                              1,    // height
                              3,    // numberOfChannels
                             32,    // bitsPerSample
                             true,  // floatSample
                             true,  // color
                             true,  // initialProcessing
                             baseId );  // imageId

   ImageVariant t = targetWindow.MainView().Image();
   Image& target = static_cast<Image&>( *t );

   StandardStatus status;
   target.SetStatusCallback( &status );

   Console().EnableAbort();

   DoSuperPixel( target, source );
   */
   FITSKeywordArray keywords;
   view.Window().GetKeywords( keywords );

   keywords.Add( FITSHeaderKeyword( "COMMENT", IsoString(), "CFA2RGBing with "  + PixInsightVersion::AsString() ) );
   keywords.Add( FITSHeaderKeyword( "HISTORY", IsoString(), "CFA2RGBing with "  + Module->ReadableVersion() ) );
   keywords.Add( FITSHeaderKeyword( "HISTORY", IsoString(), "CFA2RGB.pattern: " + patternId ) );
   view.Window().SetKeywords(keywords);
   /*
   targetWindow.SetKeywords( keywords );

   targetWindow.Show();

   o_imageId = targetWindow.MainView().Id();
   */
   return true;
}

void CFA2RGBInstance::DoSuperPixel( Image& target, const ImageVariant& source )
{
   if ( source.IsFloatSample() )
      switch ( source.BitsPerSample() )
      {
      case 32: CFA2RGBEngine::SuperPixelThreaded( target, static_cast<const Image&>( *source ), *this ); break;
      case 64: CFA2RGBEngine::SuperPixelThreaded( target, static_cast<const DImage&>( *source ), *this ); break;
      }
   else
      switch ( source.BitsPerSample() )
      {
      case  8: CFA2RGBEngine::SuperPixelThreaded( target, static_cast<const UInt8Image&>( *source ), *this ); break;
      case 16: CFA2RGBEngine::SuperPixelThreaded( target, static_cast<const UInt16Image&>( *source ), *this ); break;
      case 32: CFA2RGBEngine::SuperPixelThreaded( target, static_cast<const UInt32Image&>( *source ), *this ); break;
      }
}

// ----------------------------------------------------------------------------

void* CFA2RGBInstance::LockParameter( const MetaParameter* p, size_type /*tableRow*/ )
{
   if ( p == TheCFA2RGBBayerPatternParameter )
      return &p_bayerPattern;
   if ( p == TheCFA2RGBOutputImageParameter )
      return o_imageId.c_str();
 
   return 0;
}

bool CFA2RGBInstance::AllocateParameter( size_type sizeOrLength, const MetaParameter* p, size_type tableRow )
{
   if ( p == TheCFA2RGBOutputImageParameter )
   {
      o_imageId.Clear();
      if ( sizeOrLength > 0 )
         o_imageId.Reserve( sizeOrLength );
   }
   else
      return false;

   return true;
}

size_type CFA2RGBInstance::ParameterLength( const MetaParameter* p, size_type tableRow ) const
{
   if ( p == TheCFA2RGBOutputImageParameter )
      return o_imageId.Length();

   return 0;
}

// ----------------------------------------------------------------------------

} // pcl

// ****************************************************************************
// EOF CFA2RGBInstance.cpp - Released 2014/10/29 07:35:26 UTC
