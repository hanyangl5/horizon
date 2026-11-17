/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "Core/ILog.h"

#include "AssetPipeline.h"

const char* gApplicationName = "AssetPipeline";

const AssetPipelineProcessCommand gAssetPipelineCommands[] = {
    { "-pgltf", PROCESS_GLTF },
};

void PrintHelp()
{
    printf("Asset Pipeline\n");
    printf("\n-command [Flags]\n");
    printf("\nCommands:\n");
    printf("\n\t%s\t(GLTF to bin)\tProcessGLTF\n", gAssetPipelineCommands[PROCESS_GLTF].mCommandString);
    printf("\n\t\tAlso emits .sceneasset.json, .assetcatalog.json and BC7 DDS textures\n");
    printf("\n\t\t--optimize\t\t: Enables all supported mesh optimization techniques\n");
    printf("\n\t\t--optimizecache\t\t: Enables vertex cache optimization\n");
    printf("\n\t\t--optimizeoverdraw\t\t: Enables overdraw optimization\n");
    printf("\n\t\t--optimizefetch\t\t: Enables vertex fetch optimization\n");
    printf(
        "\n\t\t--meshlets\t\t: Enables meshlet generation. By default number of vertices and triangles in Each meshlet is limited by 128 "
        "and 256 respectively\n");
    printf("\n\t\t--meshletnumvertices [num]\t\t: Overrides maximum number of vertices in Each meshlet\n");
    printf("\n\t\t--meshletnumtriangles [num]\t\t: Overrides maximum number of triangles in Each meshlet\n");
    printf("\nCommon Options:\n");
    printf("\n\t-h | -help\t\t: Print usage information\n");
    printf("\n\t--input [path]\t\t\t: Choose input folder\n");
    printf("\n\t--output [path]\t\t\t: Choose output folder\n");
    printf("\n\t--quiet\t\t\t: Print only error messages\n");
    printf("\n\t--force\t\t\t: Force all assets to be processed\n");
}

int AssetPipelineCmd(int argc, char** argv)
{
    if (argc == 1)
    {
        PrintHelp();
        return 0;
    }

    if (stricmp(argv[1], "-h") == 0 || stricmp(argv[1], "-help") == 0)
    {
        PrintHelp();
        return 0;
    }

    if (argc < 2)
    {
        printf("Invalid command.\n");
        return 0;
    }

    if (!InitMemoryAlloc(gApplicationName))
        return EXIT_FAILURE;

    const char* input = "";
    const char* output = "";

    // Parse commands, fill params
    AssetPipelineParams params = {};
    params.mSettings.force = false;
    params.mSettings.quiet = false;
    params.mInFilePath = input;
    params.mInExt = "";
    params.mFlagsCount = 0;
    params.mPathMode = PROCESS_MODE_NONE;

    params.mRDInput = DirectoryMiddleware1;
    params.mRDOutput = DirectoryMiddleware2;

    char filePath[FS_MAX_PATH] = { 0 };
    char fileNameWithoutExt[FS_MAX_PATH] = { 0 };
    char fileName[FS_MAX_PATH] = { 0 };
    char iext[FS_MAX_PATH] = { 0 };

    for (int i = 2; i < argc; ++i)
    {
        const char* arg = argv[i];

        if (STRCMP(arg, "--input"))
        {
            if (params.mPathMode == PROCESS_MODE_NONE)
            {
                params.mPathMode = PROCESS_MODE_DIRECTORY;
            }
            input = argv[++i];
        }
        else if (STRCMP(arg, "--input-file"))
        {
            if (params.mPathMode == PROCESS_MODE_DIRECTORY_RECURSIVE)
            {
                LOGF(Error, "--recursive flag not allowed on single file mode");
                return 1;
            }

            i++;
            params.mPathMode = PROCESS_MODE_FILE;
            GetPathFileName(argv[i], fileNameWithoutExt);
            GetPathExtension(argv[i], iext);
            params.mInExt = iext;
            AppendPathExtension(fileNameWithoutExt, iext, fileName);
            params.mInFilePath = fileName;

            GetParentPath(argv[i], filePath);
            input = filePath;
        }
        else if (STRCMP(arg, "--recursive"))
        {
            if (params.mPathMode == PROCESS_MODE_FILE)
            {
                LOGF(Error, "--recursive flag not allowed on single file mode");
                return 1;
            }

            params.mPathMode = PROCESS_MODE_DIRECTORY_RECURSIVE;
        }
        else if (STRCMP(arg, "--output"))
        {
            output = argv[++i];
        }
        else if (STRCMP(arg, "--quiet"))
        {
            params.mSettings.quiet = true;
        }
        else if (STRCMP(arg, "--force"))
        {
            params.mSettings.force = true;
        }
        else
        {
            params.mFlags[params.mFlagsCount++] = argv[i];
        }
    }

    params.mInDir = input;
    params.mOutDir = output;

    FileSystemInitDesc FileSystemDesc = {};
    FileSystemDesc.pAppName = gApplicationName;
    FileSystemDesc.pResourceMounts[MountContent] = input;
    FileSystemDesc.pResourceMounts[MountProject] = output;

    if (!InitFileSystem(&FileSystemDesc))
    {
        LOGF(Error, "Filesystem failed to initialize.");
        ExitMemoryAlloc();
        return 1;
    }

    SetResourceDirectoryPath(SystemFileIO, MountContent, params.mRDInput, "");
    SetResourceDirectoryPath(SystemFileIO, MountProject, params.mRDOutput, "");
    SetResourceDirectoryPath(SystemFileIO, MountDebug, DirectoryLog, "");

    LogLevel logLevel = params.mSettings.quiet ? Warning : DEFAULT_LOG_LEVEL;
    InitLog(gApplicationName, logLevel);

    bool runAssetPipeline = false;
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(gAssetPipelineCommands); ++i)
    {
        if (STRCMP(argv[1], gAssetPipelineCommands[i].mCommandString))
        {
            params.mProcessType = gAssetPipelineCommands[i].mProcessType;
            runAssetPipeline = true;
            break;
        }
    }

    // ...

    int ret = 0;

    if (runAssetPipeline)
    {
        // Make sure output folder exists before starting the pipeline
        if (MakeDirectory(params.mRDOutput, "", true))
        {
            ret = AssetPipelineRun(&params);
        }
    }
    else
    {
        LOGF(Error, "Unrecognized argument %s.", argv[1]);
    }

    ExitLog();
    ExitFileSystem();
    ExitMemoryAlloc();

    return ret;
}

int main(int argc, char** argv) { return AssetPipelineCmd(argc, argv); }
