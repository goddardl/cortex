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

#include "IECore/DeepImageReader.h"
#include "IECore/DeepPixel.h"

#include "IECoreNuke/DeepImageReader.h"

#include "DDImage/IopInfo.h"

#include "boost/algorithm/string.hpp"
#include "boost/regex.hpp"

namespace IECoreNuke
{

static const char* const CLASS = "ieDeepImageReader";

static const char* const HELP = "Reads any deep image format that is registered with Cortex.";

DD::Image::Op *DeepImageReader::build( Node *node ){ return new DeepImageReader( node ); }

const DD::Image::DeepFilterOp::Description DeepImageReader::m_description( CLASS, DeepImageReader::build );

const char *DeepImageReader::Class() const { return m_description.name; }

const char *DeepImageReader::node_help() const { return HELP; }

DeepImageReader::DeepImageReader( Node *node ) :
	DD::Image::DeepFilterOp( node ),
	m_currentPath( "" ),
	m_reload( 1 ),
	m_file( "" ),
	m_firstFrame( 1 ),
	m_lastFrame( 1 ),
	m_onError( Error )
{
	m_formats.format(0);
	inputs(0); // No inputs.
}

DeepImageReader::~DeepImageReader()
{
}

void DeepImageReader::append( DD::Image::Hash &hash )
{
	hash.append( m_file );
	hash.append( m_dataWindow.x() );
	hash.append( m_dataWindow.y() );
	hash.append( m_dataWindow.r() );
	hash.append( m_dataWindow.t() );
	hash.append( m_channels );
	hash.append( m_currentPath );
	hash.append( m_reload );

	std::vector<std::string> extensions;
	IECore::Reader::supportedExtensions( extensions );

	hash.append( int( extensions.size() ) );
}

void DeepImageReader::getDeepRequests( DD::Image::Box box, const DD::Image::ChannelSet &channels, int count, std::vector<DD::Image::RequestData> &requests )
{
	DeepFilterOp::getDeepRequests( box, channels, count, requests );
}

int DeepImageReader::knob_changed( DD::Image::Knob *k )
{
	if( k->is( "first" ) )
	{
		m_firstFrame = int( k->get_value() );
		return 1;
	}
	else if( k->is( "last" ) )
	{
		m_lastFrame = int( k->get_value() );
		return 1;
	}
	else if( k->is( "reload" ) )
	{
		m_reload += 1;
		return 1;
	}

	return DeepFilterOp::knob_changed(k);
}

void DeepImageReader::knobs( DD::Image::Knob_Callback f )
{
	File_knob( f, &m_file, "file" );
	SetFlags( f, DD::Image::Knob::KNOB_CHANGED_ALWAYS );
	SetFlags( f, DD::Image::Knob::ALWAYS_SAVE );
	SetFlags( f, DD::Image::Knob::NO_UNDO );
	Tooltip( f, "Path of the deep image to read." );

	Format_knob(f, &m_formats, "format" );
    Obsolete_knob(f, "full_format", "knob format $value" );
	Obsolete_knob(f, "proxy_format", 0 );

	Int_knob( f, &m_firstFrame, "first", "frame range" );
	SetFlags( f, DD::Image::Knob::NO_ANIMATION );
	Int_knob( f, &m_lastFrame, "last", "" );
	SetFlags( f, DD::Image::Knob::NO_ANIMATION );
	
	static const char * const onError[] = { "Error", "Use Nearest Frame", "Black", 0 };
	Enumeration_knob( f, &m_onError, onError, "onError", "Missing Frames" );
	Tooltip( f, "How missing files from the sequence are handled." );
	SetFlags( f, DD::Image::Knob::KNOB_CHANGED_ALWAYS );
	SetFlags( f, DD::Image::Knob::ALWAYS_SAVE );
	Button( f, "reload" );
}

void DeepImageReader::_validate( bool for_real )
{
	DeepFilterOp::_validate( for_real );
	
	DD::Image::IopInfo info;
	if( m_formats.format() )
	{
		info.format( *m_formats.format() );
		info.full_size_format( *m_formats.fullSizeFormat() );
	}

	info.first_frame( m_firstFrame );
	info.last_frame( m_lastFrame );
	info.set( *m_formats.format() );

	std::string errorMsg;
	if( loadFile( errorMsg ) )
	{
		info.turn_on( m_channels );
		info.set( DD::Image::Box( m_dataWindow.x(), m_dataWindow.y(), m_dataWindow.r(), m_dataWindow.t() ) );

		int width = m_reader->displayWindow().size().x + 1;
		int height = m_reader->displayWindow().size().y + 1;
		DD::Image::Format *format = DD::Image::Format::findExisting( width, height );
		if( !format )
		{
			format = new DD::Image::Format( width, height );
			format->add( NULL );
		}
		info.format( *format );
	}
	else
	{
		if( m_onError == Error || m_onError == NearestFrame )
		{
			error( errorMsg.c_str() );
		}
		else if( m_onError == Black )
		{
			// Do nothing as we still want doDeepEngine to be called
			// so that it just draws a black image.
		}
	}

	_deepInfo = DD::Image::DeepInfo( info );
}

bool DeepImageReader::doDeepEngine( DD::Image::Box box, const DD::Image::ChannelSet &channels, DD::Image::DeepOutputPlane &plane )
{
	plane = DD::Image::DeepOutputPlane( channels, box );

	if( !m_reader || m_channels.empty() )
	{
		for( int y = box.y(); y < box.t(); ++y )
		{
			for( int ox = box.x(); ox != box.r(); ++ox )
			{
				plane.addHole();
			}
		}

		return true;
	}

	DD::Image::Guard g( m_lock );
	Imath::Box2i displayWindow( m_reader->displayWindow() );	

	for( int y = box.y(); y < box.t(); ++y )
	{
		if( y < m_dataWindow.y() || y >= m_dataWindow.t() )
		{
			for( int ox = box.x(); ox != box.r(); ++ox )
			{
				plane.addHole();
			}
			continue;
		}

		int minX = std::min( m_dataWindow.x(), box.x() );
		int maxX = std::max( m_dataWindow.r(), box.r() );

		// The Y coordinate in the cortex deep image coordinate space.	
		int cy = displayWindow.size().y - ( y - displayWindow.min[1] );

		for( int x = minX; x < maxX; ++x )
		{
			if( x < m_dataWindow.x() || x >= m_dataWindow.r() )
			{
				plane.addHole();
				continue;
			}

			unsigned int nSamples( 0 );

			IECore::DeepPixelPtr pixel;
			try
			{
				pixel = m_reader->readPixel( x, cy );
				if( pixel )
				{
					nSamples = pixel->numSamples(); 
				}
			}
			catch( const IECore::Exception &e )
			{
			}

			if( nSamples == 0 )
			{
				if( x >= box.x() || x < box.r() )
				{
					plane.addHole();
				}
				continue;
			}

			if( x < box.x() || x >= box.r() )
			{	
				continue;
			}
		
			DD::Image::DeepOutPixel dop;
			for( unsigned int i = 0; i < nSamples; ++i )
			{
				float *data( pixel->channelData( i ) );

				DD::Image::ChannelSet allChans = m_channels + channels;
				foreach( z, allChans )
				{
					if( z == DD::Image::Chan_DeepFront || z == DD::Image::Chan_DeepBack )
					{
						dop.push_back( pixel->getDepth( i ) );
					}
					else
					{
						if( m_channels.contains(z) )
						{
							dop.push_back( data[ m_channelMap[z] ] );
						}
						else
						{
							dop.push_back(0);
						}
					}
				}
			}
			plane.addPixel( dop );
		}
	}
	return true;
}

bool DeepImageReader::loadFileFromPath( const std::string &filePath, std::string &errorMsg )
{
	try
	{
		// Perform an early-out if we have already loaded the desired file.
		if( m_currentPath == filePath && m_reader )
		{
			return true;
		}

		IECore::ReaderPtr object( IECore::Reader::create( filePath ) );
		m_reader = IECore::runTimeCast<IECore::DeepImageReader>( object );
		
		if( m_reader )
		{
			m_currentPath = filePath;
			std::vector< std::string > channelNames;

			m_reader->channelNames( channelNames );

			m_channels.clear();
			m_channelMap.clear();
			for( std::vector< std::string >::const_iterator it( channelNames.begin() ); it != channelNames.end(); ++it )
			{
				int idx( it - channelNames.begin() ); 
				if( *it == "A" )
				{
					m_channels += DD::Image::Chan_Alpha;
					m_channelMap[DD::Image::Chan_Alpha] = idx; 
				}
				else if( *it == "R" )
				{
					m_channels += DD::Image::Chan_Red;
					m_channelMap[DD::Image::Chan_Red] = idx; 
				}
				else if( *it == "G" )
				{
					m_channels += DD::Image::Chan_Green;
					m_channelMap[DD::Image::Chan_Green] = idx; 
				}
				else if( *it == "B" )
				{
					m_channels += DD::Image::Chan_Blue;
					m_channelMap[DD::Image::Chan_Blue] = idx; 
				}
			}
			
			Imath::Box2i dataWindow( m_reader->dataWindow() ); 
			m_dataWindow = DD::Image::Box( dataWindow.min[0], dataWindow.min[1], dataWindow.max[0]+1, dataWindow.max[1]+1 );
			errorMsg = "";
			return true;
		}
		else
		{
			errorMsg = "Object is not an IECore::DeepImageReader.";
		}
	}
	catch( const IECore::Exception &e )
	{
		errorMsg = ( boost::format( "DeepImageReader : %s" ) % e.what() ).str();
	}
	return false;
}

bool DeepImageReader::loadFile( std::string &errorMsg )
{
	errorMsg = "";

	// Check that the returnPath isn't null.
	std::string path = filePath( outputContext() );
	if( path == "" )
	{
		m_reader = NULL; // Invalidate the current file reader.
		return false;
	}

	// Get the returnPath required for the current context.
	const int currentFrame = int( roundf( outputContext().frame() ) );
	
	if( m_onError == Error || m_onError == Black )
	{
		return loadFileFromPath( path, errorMsg );
	}
	else if( m_onError == NearestFrame )
	{
		DD::Image::OutputContext context( outputContext() );

		if( currentFrame <= m_firstFrame )
		{
			for( int frame = m_firstFrame; frame <= m_lastFrame; ++frame ) 
			{
				context.setFrame( frame );
				if( loadFileFromPath( filePath( context ), errorMsg ) )
				{
					return true;
				}
			}
		}
		else if( currentFrame >= m_lastFrame )
		{
			for( int frame = m_lastFrame; frame >= m_firstFrame; --frame ) 
			{
				context.setFrame( frame );
				if( loadFileFromPath( filePath( context ), errorMsg ) )
				{
					return true;
				}
			}
		}
		else
		{
			int downFrame = currentFrame;
			int upFrame = currentFrame+1;
			for( ; downFrame >= m_firstFrame || upFrame <= m_lastFrame; --downFrame, ++upFrame )
			{
				if( downFrame >= m_firstFrame )
				{
					context.setFrame( downFrame );
					if( loadFileFromPath( filePath( context ), errorMsg ) )
					{
						return true;
					}
				}
				if( upFrame <= m_lastFrame )
				{
					context.setFrame( upFrame );
					if( loadFileFromPath( filePath( context ), errorMsg ) )
					{
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

std::string DeepImageReader::filePath( const DD::Image::OutputContext &context ) const
{
	std::string path;

	if ( knob("file") != NULL )
	{
		std::stringstream pathStream;
		knob("file")->to_script( pathStream, &context, false );
		path = pathStream.str();
	}
	
	if( path == "" )
	{
		return path;
	}

	// Get the path required for the current context.
	const float frame = outputContext().frame();
	if ( !strchr( path.c_str(), '#' ) )
	{
		boost::algorithm::replace_first( path, "%07d", "%07i" );
		boost::algorithm::replace_first( path, "%06d", "%06i" );
		boost::algorithm::replace_first( path, "%05d", "%05i" );
		boost::algorithm::replace_first( path, "%04d", "%04i" );
		boost::algorithm::replace_first( path, "%03d", "%03i" );
		boost::algorithm::replace_first( path, "%02d", "%02i" );
		boost::algorithm::replace_first( path, "%d", "%1i" );
	}
	else
	{
		boost::algorithm::replace_first( path, "#######", "%07i" );
		boost::algorithm::replace_first( path, "######", "%06i" );
		boost::algorithm::replace_first( path, "#####", "%05i" );
		boost::algorithm::replace_first( path, "####", "%04i" );
		boost::algorithm::replace_first( path, "###", "%03i" );
		boost::algorithm::replace_first( path, "##", "%02i" );
		boost::algorithm::replace_first( path, "#", "%1i" );
	}

	try
	{
		path = boost::str( boost::format( path ) % int(frame) );
	}
	catch( ... )
	{
		// It's ok if the above throws - it just means there wasn't a number specified in the
		// filePath, so we just use the filePath as is.
	}

	return path;
}

} // namespace IECoreNuke
