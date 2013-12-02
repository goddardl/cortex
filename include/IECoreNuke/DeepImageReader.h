//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#ifndef IECORENUKE_DEEPIMAGEREADER_H
#define IECORENUKE_DEEPIMAGEREADER_H

#include "DDImage/Knobs.h"
#include "DDImage/Thread.h"
#include "DDImage/DeepFilterOp.h"
#include "DDImage/DeepPlane.h"

namespace IECoreNuke
{

/// Reads various DeepImage formats.
/// The DeepImageReader supports the reading of any deep image file that has an associated reader registered with cortex.
class DeepImageReader : public DD::Image::DeepFilterOp
{

	public:
		
		enum
		{
			Error = 0,
			NearestFrame = 1,
			Black = 2
		};

		DeepImageReader( Node *op );
		~DeepImageReader();

		virtual bool doDeepEngine( DD::Image::Box box, const DD::Image::ChannelSet &channels, DD::Image::DeepOutputPlane &plane );
		virtual void getDeepRequests( DD::Image::Box box, const DD::Image::ChannelSet &channels, int count, std::vector<DD::Image::RequestData> &requests );
		virtual void knobs( DD::Image::Knob_Callback );
		virtual int knob_changed( DD::Image::Knob *k );
		virtual void _validate( bool for_real );

		virtual const char *Class() const;
		virtual const char *node_help() const;
		virtual void append( DD::Image::Hash &hash );

		static const DD::Image::DeepFilterOp::Description m_description;
		static DD::Image::Op *build( Node *node );

	private:

		/// Loads an image and sets m_reader to the reader for the file. If an Exception is
		/// raised then the reason is saved and returned in errorMsg.
		/// Returns true if the file was successfully loaded.
		bool loadFileFromPath( const std::string &filePath, std::string &errorMsg );
		/// Loads an image either specified by the "file" knob or if it fails, the file
		/// specified by the "onError" knob. 
		/// Returns true if the file was successfully loaded.
		bool loadFile( std::string &errorMsg );

		/// Returns the file path for a given context. This method will
		/// perform wildcard substitution on the path with the frame number
		/// specified within the context.
		std::string filePath( const DD::Image::OutputContext &context ) const;

		/// Holds the path of the file that is currently being read. 
		std::string m_currentPath;
		/// A counter that we append to the hash. It is incremented by the
		/// "reload" button to force an update.
		int m_reload;
		/// A mutex which ensures that only one thread reads from the file at once.	
		DD::Image::Lock m_lock;
		/// The data window of the file. This is set within loadFileFromPath().
		DD::Image::Box m_dataWindow;
		/// The channels within the file. This is set within loadFileFromPath().
		DD::Image::ChannelSet m_channels;
		/// The cortex reader that we use to read the file.
		IECore::DeepImageReaderPtr m_reader;
		/// A map of Channels to indexes within the IECore::DeepPixel class's channelData().
		std::map< DD::Image::Channel, int > m_channelMap;
		
		//! @name Knob Data Members
		/// Various members that are used by knobs to hold their value.
		//@{
		const char *m_file;
		int m_firstFrame;
		int m_lastFrame;
		int m_onError;
		DD::Image::FormatPair m_formats;
		//@}	
};

}; // namespace IECoreNuke


#endif
