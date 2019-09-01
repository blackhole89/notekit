##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=notekit
ConfigurationName      :=Debug
WorkspacePath          :=/d0/coding/notekit
ProjectPath            :=/d0/coding/notekit
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=Matvey Soloviev
Date                   :=01/09/19
CodeLitePath           :=/home/negativezero/.codelite
LinkerName             :=/usr/bin/g++
SharedObjectLinkerName :=/usr/bin/g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="notekit.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            := $(shell pkg-config --libs gtkmm-3.0) $(shell pkg-config --libs gtksourceviewmm-3.0) $(shell pkg-config webkit2gtk-4.0 --libs) $(shell pkg-config --libs jsoncpp) 
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)z 
ArLibs                 :=  "z" 
LibPath                := $(LibraryPathSwitch). 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/ar rcu
CXX      := /usr/bin/g++
CC       := /usr/bin/gcc
CXXFLAGS := $(shell pkg-config --cflags gtkmm-3.0) $(shell pkg-config --cflags gtksourceviewmm-3.0) $(shell pkg-config --cflags jsoncpp) -g -Wall -O0 $(Preprocessors)
CFLAGS   :=  -g -O0 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/main.cpp$(ObjectSuffix) $(IntermediateDirectory)/navigation.cpp$(ObjectSuffix) $(IntermediateDirectory)/notebook.cpp$(ObjectSuffix) $(IntermediateDirectory)/mainwindow.cpp$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@test -d ./Debug || $(MakeDirCommand) ./Debug


$(IntermediateDirectory)/.d:
	@test -d ./Debug || $(MakeDirCommand) ./Debug

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/main.cpp$(ObjectSuffix): main.cpp $(IntermediateDirectory)/main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/d0/coding/notekit/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/main.cpp$(DependSuffix): main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/main.cpp$(DependSuffix) -MM main.cpp

$(IntermediateDirectory)/main.cpp$(PreprocessSuffix): main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/main.cpp$(PreprocessSuffix) main.cpp

$(IntermediateDirectory)/navigation.cpp$(ObjectSuffix): navigation.cpp $(IntermediateDirectory)/navigation.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/d0/coding/notekit/navigation.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/navigation.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/navigation.cpp$(DependSuffix): navigation.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/navigation.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/navigation.cpp$(DependSuffix) -MM navigation.cpp

$(IntermediateDirectory)/navigation.cpp$(PreprocessSuffix): navigation.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/navigation.cpp$(PreprocessSuffix) navigation.cpp

$(IntermediateDirectory)/notebook.cpp$(ObjectSuffix): notebook.cpp $(IntermediateDirectory)/notebook.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/d0/coding/notekit/notebook.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/notebook.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/notebook.cpp$(DependSuffix): notebook.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/notebook.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/notebook.cpp$(DependSuffix) -MM notebook.cpp

$(IntermediateDirectory)/notebook.cpp$(PreprocessSuffix): notebook.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/notebook.cpp$(PreprocessSuffix) notebook.cpp

$(IntermediateDirectory)/mainwindow.cpp$(ObjectSuffix): mainwindow.cpp $(IntermediateDirectory)/mainwindow.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/d0/coding/notekit/mainwindow.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/mainwindow.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/mainwindow.cpp$(DependSuffix): mainwindow.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/mainwindow.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/mainwindow.cpp$(DependSuffix) -MM mainwindow.cpp

$(IntermediateDirectory)/mainwindow.cpp$(PreprocessSuffix): mainwindow.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/mainwindow.cpp$(PreprocessSuffix) mainwindow.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


