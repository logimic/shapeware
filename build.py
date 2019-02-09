# 
# Copyright 2018 Logimic,s.r.o.
# www.logimic.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import sys, os
import pathlib
import shutil
import argparse
from subprocess import call

mainDir  = ""

def main():

    parser = argparse.ArgumentParser(description='Building script for Shape.')
    parser.add_argument('-G','--gen', help='Generator. If you do not know call command \"cmake -G\"', required=False)
    # parser.add_argument('-D', action='append', nargs='*', help='Description for bar argument', required=False)    
    args = parser.parse_args()

    if not args.gen:
        print("Generator not specified, try use: -h")
        return None

    buildFolder = "Unknown_Builder"

    if args.gen:
        print("Generator:" + args.gen)
        if args.gen == "Unix Makefiles":
            buildFolder = "Unix_Makefiles"
        elif args.gen == "Visual Studio 15 2017 Win64":
            buildFolder = "VS15_2017_x64"  
        elif args.gen == "Visual Studio 15 2017":
            buildFolder = "VS15_2017"       

    buildDir = os.path.normpath(mainDir + "/build/" + buildFolder) 
    
    # Create build dir
    if not os.path.exists(buildDir):
        os.makedirs(buildDir)   

    generator = " -G \"" + args.gen +  "\""     

    # Shape
    os.chdir("..")
    shape = os.path.normpath(os.getcwd() + "/shape/" +  "/build/" + buildFolder)

    vcpkg = ""
    if args.gen == "Unix Makefiles":
        vcpkg = ""
    elif args.gen == "Visual Studio 15 2017 Win64":
        vcpkg = os.path.normpath("c:/devel/vcpkg/scripts/buildsystems/vcpkg.cmake")    
    elif args.gen == "Visual Studio 15 2017":
        vcpkg = os.path.normpath("c:/devel/vcpkg/scripts/buildsystems/vcpkg.cmake")            
    else:
        vcpkg = ""

    dTesting = "-DBUILD_TESTING:BOOL=true"    
    dShape= "-Dshape_DIR:PATH=" + shape
    dSsl = "-DLWS_WITH_SSL:BOOL=false"
    dToolChain = "-DCMAKE_TOOLCHAIN_FILE=" + vcpkg
    dDebug = "-DCMAKE_BUILD_TYPE=Debug"
    dRelease = "-DCMAKE_BUILD_TYPE=Release"

    # Building
    if args.gen == "Unix Makefiles":
        command = "cmake" + generator + " " + dTesting + " " + dDebug + " " + dToolChain + " " + dSsl + " " + dShape + " " + mainDir        
        
        print("command: " + command)  
        os.chdir(buildDir)   
        os.system(command)    

        os.chdir(mainDir)
        command = "cmake --build " + buildDir + " --config Debug --target install"
        os.system(command)  

        command = "cmake" + generator + " " + dTesting + " " + dRelease + " " + dToolChain + " " + dSsl + " " + dShape + " " + mainDir        
        
        print("command: " + command)  
        os.chdir(buildDir)   
        os.system(command)            

        command = "cmake --build " + buildDir + " --config Release --target install"
        os.system(command)     

    else: 
        command = "cmake" + generator + " " + dTesting + " " + dToolChain + " " + dSsl + " " + dShape + " " + mainDir        
        
        print("command: " + command)  
        os.chdir(buildDir)   
        os.system(command)    

        os.chdir(mainDir)
        command = "cmake --build " + buildDir + " --config Debug --target install"
        os.system(command)  

        command = "cmake --build " + buildDir + " --config Release --target install"
        os.system(command)             

if __name__ == "__main__":
    mainDir  = os.getcwd()
    main()