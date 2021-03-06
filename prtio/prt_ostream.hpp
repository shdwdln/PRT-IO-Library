/**
 * Copyright 2012 Thinkbox Software Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file contains the interface for writing streams of prt data.
 */

#pragma once

#include <prtio/detail/conversion.hpp>
#include <prtio/detail/data_types.hpp>
#include <prtio/prt_layout.hpp>
#include <prtio/prt_meta_value.hpp>

#include <exception>
#include <string>
#include <sstream>
#include <vector>

namespace prtio{

/**
 * This class defines the interface for a user writing particle data to a prt stream. Subclasses of this will
 * implement the specific destination of the prt data. Ex. prt_ofstream writes to a file.
 */
class prt_ostream{
	/**
	 * This internal class is used for storing information about how to fill a channel for a particle.
	 */
	struct bound_channel{
		void* src;
		std::size_t arity, dest;
		detail::convert_fn_t copyFn;
	};

	//A list of all channels that we want to extract
	std::vector< bound_channel > m_boundChannels;

protected:
	//The layout of the particle data from the source (ex. PRT file).
	prt_layout m_layout;
	
	std::map< std::string, prt_meta_value > m_fileMetadata;
	std::map< std::string, std::map< std::string, prt_meta_value > > m_channelMetadata;

	/**
	 * This abstract function provides the interface for subclasses to consume a single particle.
	 * When prt_ostream::write_next_particle() is called, it uses write_impl() to commit the next
	 * particle to the destination.
	 * @param src A pointer to a single particle with layout described by 'm_layout'.
	 */
	virtual void write_impl( const char* src ) = 0;

public:
	prt_ostream()
	{}

	virtual ~prt_ostream()
	{}
	
	/**
	 * Adds a metadata name/value pair associated with the entire file.
	 * @param name The name associated with the metadata.
	 * @param value The generic value associated with the metadata.
	 */
	void add_file_metadata( const std::string& name, const prt_meta_value& value ){
		m_fileMetadata[ name ] = value;
	}

	/**
	 * Adds a metadata name/value pair associated with a specific channel.
	 * @param channelName The name of the channel to associate the metadata with.
	 * @param name The name associated with the metadata.
	 * @param value The generic value associated with the metadata.
	 */
	void add_channel_metadata( const std::string& channelName, const std::string& name, const prt_meta_value& value ){
		m_channelMetadata[ channelName ][ name ] = value;
	}
	
	/**
	 * This template function will bind a user-supplied variable to a named channel to be written to a prt_ostream
	 * @tparam T The type of the variable to bind to.
	 * @param name The name of the channel in the prt_ostream to bind to.
	 * @param src A pointer to where the channel data will be read from.
	 * @param arity The size of the array pointed to by 'src'.
	 * @param destType An optional override on the type of data, for storing the stream data as a different type. Ex. Convert float to half on disk.
	 */
	template <typename T>
	void bind( const std::string& name, T src[], std::size_t arity, data_types::enum_t destType = data_types::traits<T>::data_type() ){
		if( m_layout.has_channel( name ) )
			throw std::logic_error( "Channel \"" + name + "\" is already bound" );

		if( !detail::is_compatible( destType, data_types::traits<T>::data_type() ) ){
			std::stringstream ss;
			ss << "Incompatible types for channel \"" << name << "\"";
			ss << ", cannot convert from type: \"" << data_types::names[ data_types::traits<T>::data_type() ] << "\"";
			ss << "to: \"" << data_types::names[ destType ] << "\"";

			throw std::logic_error( ss.str() );
		}

		std::size_t destOffset = m_layout.size();

		m_layout.add_channel( name, destType, arity, destOffset );

		bound_channel result;
		result.src = src;
		result.dest = destOffset;
		result.arity = arity;
		result.copyFn = detail::get_write_converter<T>( destType );

		if( !result.copyFn )
			throw std::logic_error( "The requested output type: \"" + std::string(data_types::names[ destType ]) + "\" for channel\"" + name + "\" was unsupported." );

		m_boundChannels.push_back( result );
	}

	/**
	 * This extracts the next particle's channel data from the variables supplied to bind(), then commits the particle to the stream.
	 */
	void write_next_particle(){
		//Allocate some temporary stack space for the particle.
		char* data = (char*)alloca( m_layout.size() );

		//Go through each bound channel, grabbing the data from the ptr supplied by the user and writing into the particle.
		for( std::vector< bound_channel >::iterator it = m_boundChannels.begin(), itEnd = m_boundChannels.end(); it != itEnd; ++it )
			it->copyFn( data + it->dest, it->src, it->arity );

		this->write_impl( data );
	}
};

}//namespace prtio
