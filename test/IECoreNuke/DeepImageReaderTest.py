##########################################################################
#
#  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
#     * Neither the name of Image Engine Design nor the names of any
#       other contributors to this software may be used to endorse or
#       promote products derived from this software without specific prior
#       written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

import math
import unittest
import IECoreNuke
import IECore
import nuke
import os

class DeepImageReaderTest( IECoreNuke.TestCase ) :

	def __outputPaths( self ) :	
		return { "dtex" : "test/IECoreNuke/nukeDeepReadDtex.exr", "shw" : "test/IECoreNuke/nukeDeepReadShw.exr" }
	
	def __inputPaths( self ) :	
		return { "dtex" : "test/IECoreRI/data/dtex/coneAndSphere.dtex", "shw" : "test/IECoreRI/data/shw/coneAndSphere.shw" }

	def testReadSHW( self ) :
		import IECoreRI

		self.tearDown();	
		
		r = nuke.createNode("DeepRead")
		r["file"].setText( self.__inputPaths()["dtex"] )

		i = nuke.createNode("DeepToImage")
		i.setInput( 0, r )

		w = nuke.createNode("Write")
		w.setInput( 0, i )
		w["channels"].fromScript("A")
		w["file"].setText( self.__outputPaths()["dtex"] )

		d = nuke.createNode("ieDeepImageReader")
		d["file"].setText( self.__inputPaths()["shw"] )
		
		i2 = nuke.createNode("DeepToImage")
		i2.setInput( 0, d )
		
		w2 = nuke.createNode("Write")
		w2.setInput( 0, i2 )
		w2["channels"].fromScript("A")
		w2["file"].setText( self.__outputPaths()["shw"] )

		nuke.execute( w, 1, 1 )
		nuke.execute( w2, 1, 1 )
		
		img1 = IECore.Reader.create( self.__outputPaths["dtex"] ).read()
		img2 = IECore.Reader.create( self.__outputPaths["shw"] ).read()
		
		imageDiffOp = IECore.ImageDiffOp()
		res = imageDiffOp(
			imageA = img1,
			imageB = img2,
		)
		self.assertFalse( res.value )	
							
	def tearDown( self ) :
		paths = self.__outputPaths()
		for key in paths.keys() :
			if os.path.exists( paths[key] ) :
				os.remove( paths[key] )

if __name__ == "__main__":
    unittest.main()

