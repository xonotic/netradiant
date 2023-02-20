/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined ( INCLUDED_STRINGIO_H )
#define INCLUDED_STRINGIO_H

#include <stdlib.h>
#include <cctype>

#include "generic/vector.h"
#include "iscriplib.h"
#include "string/string.h"
#include "generic/callback.h"
#include "property.h"

inline float string_read_float( const char* string ){
	return static_cast<float>( atof( string ) );
}

inline int string_read_int( const char* string ){
	return atoi( string );
}

inline bool char_is_whitespace( char c ){
	return c == ' ' || c == '\t';
}

inline const char* string_remove_whitespace( const char* string ){
	for (;; )
	{
		if ( !char_is_whitespace( *string ) ) {
			break;
		}
		++string;
	}
	return string;
}

inline const char* string_remove_zeros( const char* string ){
	for (;; )
	{
		char c = *string;
		if ( c != '0' ) {
			break;
		}
		++string;
	}
	return string;
}

inline const char* string_remove_sign( const char* string ){
	if ( *string == '-' || *string == '+' ) { // signed zero - acceptable
		return ++string;
	}
	return string;
}

inline bool string_is_unsigned_zero( const char* string ){
	for (; *string != '\0'; ++string )
	{
		if ( *string != '0' ) {
			return false;
		}
	}
	return true;
}

inline bool string_is_signed_zero( const char* string ){
	return string_is_unsigned_zero( string_remove_sign( string ) );
}

//[whitespaces][+|-][nnnnn][.nnnnn][e|E[+|-]nnnn]
//(where whitespaces are any tab or space character and nnnnn may be any number of digits)
inline bool string_is_float_zero( const char* string ){
	string = string_remove_whitespace( string );
	if ( string_empty( string ) ) {
		return false;
	}

	string = string_remove_sign( string );
	if ( string_empty( string ) ) {
		// no whole number or fraction part
		return false;
	}

	// whole-number part
	string = string_remove_zeros( string );
	if ( string_empty( string ) ) {
		// no fraction or exponent
		return true;
	}
	if ( *string == '.' ) {
		// fraction part
		if ( *string++ != '0' ) {
			// invalid fraction
			return false;
		}
		string = string_remove_zeros( ++string );
		if ( string_empty( string ) ) {
			// no exponent
			return true;
		}
	}
	if ( *string == 'e' || *string == 'E' ) {
		// exponent part
		string = string_remove_sign( ++string );
		if ( *string++ != '0' ) {
			// invalid exponent
			return false;
		}
		string = string_remove_zeros( ++string );
		if ( string_empty( string ) ) {
			// no trailing whitespace
			return true;
		}
	}
	string = string_remove_whitespace( string );
	return string_empty( string );
}

inline double buffer_parse_floating_literal( const char*& buffer ){
	return strtod( buffer, const_cast<char**>( &buffer ) );
}

inline int buffer_parse_signed_decimal_integer_literal( const char*& buffer ){
	return strtol( buffer, const_cast<char**>( &buffer ), 10 );
}

inline int buffer_parse_unsigned_decimal_integer_literal( const char*& buffer ){
	return strtoul( buffer, const_cast<char**>( &buffer ), 10 );
}

// [+|-][nnnnn][.nnnnn][e|E[+|-]nnnnn]
inline bool string_parse_float( const char* string, float& f ){
	if ( string_empty( string ) ) {
		return false;
	}
	f = float(buffer_parse_floating_literal( string ) );
	return string_empty( string );
}

// format same as float
inline bool string_parse_double( const char* string, double& f ){
	if ( string_empty( string ) ) {
		return false;
	}
	f = buffer_parse_floating_literal( string );
	return string_empty( string );
}

// <float><space><float><space><float>
template<typename Element>
inline bool string_parse_vector3( const char* string, BasicVector3<Element>& v ){
	if ( string_empty( string ) || *string == ' ' ) {
		return false;
	}
	v[0] = float(buffer_parse_floating_literal( string ) );
	if ( *string++ != ' ' ) {
		return false;
	}
	v[1] = float(buffer_parse_floating_literal( string ) );
	if ( *string++ != ' ' ) {
		return false;
	}
	v[2] = float(buffer_parse_floating_literal( string ) );
	return string_empty( string );
}

template<typename Float>
inline bool string_parse_vector( const char* string, Float* first, Float* last ){
	if ( first != last && ( string_empty( string ) || *string == ' ' ) ) {
		return false;
	}
	for (;; )
	{
		*first = float(buffer_parse_floating_literal( string ) );
		if ( ++first == last ) {
			return string_empty( string );
		}
		if ( *string++ != ' ' ) {
			return false;
		}
	}
}

// decimal signed integer
inline bool string_parse_int( const char* string, int& i ){
	if ( string_empty( string ) ) {
		return false;
	}
	i = buffer_parse_signed_decimal_integer_literal( string );
	return string_empty( string );
}

// decimal unsigned integer
inline bool string_parse_size( const char* string, std::size_t& i ){
	if ( string_empty( string ) ) {
		return false;
	}
	i = buffer_parse_unsigned_decimal_integer_literal( string );
	return string_empty( string );
}


#define RETURN_FALSE_IF_FAIL( expression ) do{ if ( !expression ) {return false; } }while( 0 )

inline void Tokeniser_unexpectedError( Tokeniser& tokeniser, const char* token, const char* expected ){
	globalErrorStream() << Unsigned( tokeniser.getLine() ) << ":" << Unsigned( tokeniser.getColumn() ) << ": parse error at '" << ( token != 0 ? token : "#EOF" ) << "': expected '" << expected << "'\n";
}


inline bool Tokeniser_getFloat( Tokeniser& tokeniser, float& f ){
	const char* token = tokeniser.getToken();
	if ( token != 0 && string_parse_float( token, f ) ) {
		return true;
	}
	//fallback for 1.#IND 1.#INF 1.#QNAN cases, happening sometimes after rotating & often scaling with tex lock in BP mode
	else if ( token != 0 && strstr( token, ".#" ) ) {
		globalErrorStream() << "Warning: " << Unsigned( tokeniser.getLine() ) << ":" << Unsigned( tokeniser.getColumn() ) << ": expected parse problem at '" << token << "': wanted '#number'\nProcessing anyway\n";
	#define GARUX_DISABLE_QNAN_FALLBACK
	#ifndef GARUX_DISABLE_QNAN_FALLBACK
//		*strstr( token, ".#" ) = '\0';
	#endif
		return true;
	}
	Tokeniser_unexpectedError( tokeniser, token, "#number" );
	return false;
}

inline bool Tokeniser_getDouble( Tokeniser& tokeniser, double& f ){
	const char* token = tokeniser.getToken();
	if ( token != 0 && string_parse_double( token, f ) ) {
		return true;
	}
	Tokeniser_unexpectedError( tokeniser, token, "#number" );
	return false;
}

inline bool Tokeniser_getInteger( Tokeniser& tokeniser, int& i ){
	const char* token = tokeniser.getToken();
	if ( token != 0 && string_parse_int( token, i ) ) {
		return true;
	}
	Tokeniser_unexpectedError( tokeniser, token, "#integer" );
	return false;
}

inline bool Tokeniser_getSize( Tokeniser& tokeniser, std::size_t& i ){
	const char* token = tokeniser.getToken();
	if ( token != 0 && string_parse_size( token, i ) ) {
		return true;
	}
	Tokeniser_unexpectedError( tokeniser, token, "#unsigned-integer" );
	return false;
}

inline bool Tokeniser_parseToken( Tokeniser& tokeniser, const char* expected ){
	const char* token = tokeniser.getToken();
	if ( token != 0 && string_equal( token, expected ) ) {
		return true;
	}
	Tokeniser_unexpectedError( tokeniser, token, expected );
	return false;
}

inline bool Tokeniser_nextTokenIsDigit( Tokeniser& tokeniser ){
	const char* token = tokeniser.getToken();
	if ( token == 0 ) {
		return false;
	}
	char c = *token;
	tokeniser.ungetToken();
	return std::isdigit( c ) != 0;
}

template<typename TextOutputStreamType>
inline TextOutputStreamType& ostream_write( TextOutputStreamType& outputStream, const Vector3& v ){
	return outputStream << '(' << v.x() << ' ' << v.y() << ' ' << v.z() << ')';
}


template<>
struct PropertyImpl<bool, const char *> {
	static void Export(const bool &self, const Callback<void(const char *)> &returnz) {
		returnz(self ? "true" : "false");
	}

    static void Import(bool &self, const char *value) {
        self = string_equal(value, "true");
    }
};

template<>
struct PropertyImpl<int, const char *> {
	static void Export(const int &self, const Callback<void(const char *)> &returnz) {
		char buffer[16];
		sprintf(buffer, "%d", self);
		returnz(buffer);
	}

    static void Import(int &self, const char *value) {
        if (!string_parse_int(value, self)) {
            self = 0;
        }
    }
};

template<>
struct PropertyImpl<std::size_t, const char *> {
	static void Export(const std::size_t &self, const Callback<void(const char *)> &returnz) {
		char buffer[16];
		sprintf(buffer, "%u", Unsigned(self));
		returnz(buffer);
	}

    static void Import(std::size_t &self, const char *value) {
        int i;
        if (string_parse_int(value, i) && i >= 0) {
            self = i;
        } else {
            self = 0;
        }
    }
};

template<>
struct PropertyImpl<float, const char *> {
	static void Export(const float &self, const Callback<void(const char *)> &returnz) {
		char buffer[16];
		sprintf(buffer, "%g", self);
		returnz(buffer);
	}

    static void Import(float &self, const char *value) {
        if (!string_parse_float(value, self)) {
            self = 0;
        }
    }
};

template<>
struct PropertyImpl<Vector3, const char *> {
	static void Export(const Vector3 &self, const Callback<void(const char *)> &returnz) {
		char buffer[64];
		sprintf(buffer, "%g %g %g", self[0], self[1], self[2]);
		returnz(buffer);
	}

    static void Import(Vector3 &self, const char *value) {
        if (!string_parse_vector3(value, self)) {
            self = Vector3(0, 0, 0);
        }
    }
};

#endif
