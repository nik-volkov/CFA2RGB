// ****************************************************************************
// PixInsight Class Library - PCL 02.00.14.0695
// Standard CFA2RGB Process Module Version 01.01.01.0001
// ****************************************************************************
// CFA2RGBProcess.cpp - Released 2015/03/11 07:35:26 UTC
// ****************************************************************************
// This file is part of the standard CFA2RGB PixInsight module.
//
// Copyright (c) 2003-2015, Pleiades Astrophoto S.L. All Rights Reserved.
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

#include "CFA2RGBProcess.h"
#include "CFA2RGBParameters.h"
#include "CFA2RGBInstance.h"
#include "CFA2RGBInterface.h"

#include <pcl/Console.h>
#include <pcl/Arguments.h>
#include <pcl/View.h>
#include <pcl/Exception.h>

namespace pcl
{

// ----------------------------------------------------------------------------

CFA2RGBProcess* TheCFA2RGBProcess = 0;

// ----------------------------------------------------------------------------

CFA2RGBProcess::CFA2RGBProcess() : MetaProcess()
{
   TheCFA2RGBProcess = this;

   // Instantiate process parameters
   new CFA2RGBBayerPatternParameter( this );
}

// ----------------------------------------------------------------------------

IsoString CFA2RGBProcess::Id() const
{
   return "CFA2RGB";
}

// ----------------------------------------------------------------------------

IsoString CFA2RGBProcess::Category() const
{
   return "ColorSpaces,Preprocessing";
}

// ----------------------------------------------------------------------------

uint32 CFA2RGBProcess::Version() const
{
   return 0x100; // required
}

// ----------------------------------------------------------------------------

String CFA2RGBProcess::Description() const
{
   return
   "<html>"
   "<p>Convert Gray Bayer CFA image to Color Bayer RGB image.</p>"
   "</html>";
}

// ----------------------------------------------------------------------------

ProcessInterface* CFA2RGBProcess::DefaultInterface() const
{
   return TheCFA2RGBInterface;
}
// ----------------------------------------------------------------------------

ProcessImplementation* CFA2RGBProcess::Create() const
{
   return new CFA2RGBInstance( this );
}

// ----------------------------------------------------------------------------

ProcessImplementation* CFA2RGBProcess::Clone( const ProcessImplementation& p ) const
{
   const CFA2RGBInstance* instPtr = dynamic_cast<const CFA2RGBInstance*>( &p );
   return (instPtr != 0) ? new CFA2RGBInstance( *instPtr ) : 0;
}

// ----------------------------------------------------------------------------

} // pcl

// ****************************************************************************
// EOF CFA2RGBProcess.cpp - Released 2015/03/11 07:35:26 UTC
