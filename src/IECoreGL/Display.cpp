//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2007, Image Engine Design Inc. All rights reserved.
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

#include "IECoreGL/private/Display.h"
#include "IECoreGL/FrameBuffer.h"
#include "IECoreGL/ColorTexture.h"
#include "IECoreGL/DepthTexture.h"

#include "IECore/Writer.h"
#include "IECore/MessageHandler.h"
#include "IECore/FileNameParameter.h"
#include "IECore/ImagePrimitive.h"
#include "IECore/CompoundParameter.h"
#include "IECore/ClientDisplayDriver.h"

using namespace IECoreGL;

Display::Display( const std::string &name, const std::string &type, const std::string &data, const IECore::CompoundDataMap &parameters )
	:	m_name( name ), m_type( type ), m_data( data )
{
	std::cerr << "Creating openGL display with parameters:" << std::endl;
	std::cerr << "name: " << name << std::endl;
	std::cerr << "type: " << type << std::endl;
	std::cerr << "data: " << data << std::endl;
	for( IECore::CompoundDataMap::const_iterator it=parameters.begin(); it!=parameters.end(); it++ )
	{
		IECore::StringDataPtr s = IECore::runTimeCast< IECore::StringData >( it->second );
		if ( s )
		{
			std::cerr << s->readable() << ", ";
		}
		std::cerr << std::endl;
		m_parameters[it->first] = it->second->copy();
	}
}

void Display::display( ConstFrameBufferPtr frameBuffer ) const
{
	IECore::ImagePrimitivePtr image = 0;
	if( m_data=="rgba" )
	{
		image = frameBuffer->getColor()->imagePrimitive();
	}
	else if( m_data=="rgb" )
	{
		image = frameBuffer->getColor()->imagePrimitive();
		image->variables.erase( "A" );
	}
	else if( m_data=="z" )
	{
		image = frameBuffer->getDepth()->imagePrimitive();
	}
	else
	{
		IECore::msg( IECore::Msg::Warning, "Display::display", boost::format( "Unsupported data format \"%s\"." ) % m_data );
		return;
	}

	if ( m_type=="ieDisplay" )
	{
		std::vector<std::string> channelNames;
		image->channelNames( channelNames );
		Imath::Box2i dataWindow = image->getDataWindow();

		IECore::CompoundDataPtr params = new IECore::CompoundData( m_parameters );

/*		(*params)['displayHost'] = StringData('localhost')
		(*params)['displayPort'] = StringData( '1559' )
		(*params)["remoteDisplayType"] = StringData( "ImageDisplayDriver" )
		(*params)["handle"] = StringData( "myHandle" )
*/		

		IECore::StringData *driverType = params->member<IECore::StringData>( "driverType", false, false );
		
		IECore::DisplayDriverPtr driver;
		if ( driverType )
		{
			try
			{
				driver = IECore::DisplayDriver::create( driverType->readable(), image->getDisplayWindow(), dataWindow, channelNames, params );
			}
			catch( const IECore::Exception &e )
			{
			}
		}
		
		if ( !driver )
		{
			if ( !driverType )
			{
				IECore::msg( IECore::Msg::Info, "Display::display", "No \"driverType\" was specified. Creating a ClientDisplayDriver by default." );
			}
			else
			{
				IECore::msg( IECore::Msg::Warning, "Display::display", boost::format( "Failed to create a display of type \"%s\". Creating a ClientDisplayDriver by default." ) % driverType->readable() );
			}
			driver = new IECore::ClientDisplayDriver( image->getDisplayWindow(), dataWindow, channelNames, params );
		}

		const int numChannels = channelNames.size();
		unsigned int channelSize = ( dataWindow.size().x + 1 ) * ( dataWindow.size().y + 1 );
		unsigned int dataSize = channelSize * numChannels;
		std::vector<float> interleavedData( dataSize, 1.f );
		
		for( int c = 0; c < numChannels; c++ )
		{
			const IECore::TypedData< std::vector<float> > *inData = image->getChannel<float>( channelNames[c] );
			if ( !inData )
			{
				IECore::msg( IECore::Msg::Warning, "Display::display", boost::format( "Failed to get image data for channel \"%s\"." ) % channelNames[c] );
				return;
			}

			const float *in = &inData->readable()[0];
			float *out = &interleavedData[c];
			for( unsigned int i = 0; i < channelSize; i++ )
			{
				*out = *in;
				out += numChannels;
				in ++;
			}
		}
		
		driver->imageData( dataWindow, &interleavedData[0], dataSize );
		driver->imageClose();

		return;
	}

	IECore::WriterPtr writer = IECore::Writer::create( image, "tmp." + m_type );
	if( !writer )
	{
		IECore::msg( IECore::Msg::Warning, "Display::display", boost::format( "Unsupported display type \"%s\"." ) % m_type );
		return;
	}

	writer->parameters()->parameter<IECore::FileNameParameter>( "fileName" )->setTypedValue( m_name );
	writer->write();
}

