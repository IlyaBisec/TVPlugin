#include "TVPlugin.h"

TV_HANDLE *tv_handle = NULL;

TVPLUGIN_API int tv_Init()
{
    // TV_HANDLE
    tv_handle = (TV_HANDLE *)malloc(sizeof(TV_HANDLE));
    if (!tv_handle)
        return TV_FAIL;

    // Init DirectSound
    if (FAILED(DirectSoundCreate(NULL, &tv_handle->ds, NULL)))
        return TV_FAIL;

    if (FAILED(tv_handle->ds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_NORMAL)))
        return TV_FAIL;

    // Init Direct3D
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
        return TV_FAIL;

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetDesktopWindow();

    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetDesktopWindow(),
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &tv_handle->d3dDevice)))
    {
        d3d->Release();
        return TV_FAIL;
    }

    d3d->Release();
    return TV_OK;
}


TVPLUGIN_API int tv_Done()
{
    // Complete work DirectSound & Direct3D
    if (tv_handle)
    {
        if (tv_handle->ds) tv_handle->ds->Release();
        if (tv_handle->d3dDevice) tv_handle->d3dDevice->Release();

        free(tv_handle);
        tv_handle = NULL;
    }

    return TV_OK;
}


TVPLUGIN_API TV_AH tv_StreamOpen(const char *pcszFileName)
{
    TV_AH handle = (TV_AH)malloc(sizeof(TV_HANDLE)); ;
    if (ov_fopen(pcszFileName, &handle->vorbisFile) < 0)
    {
        delete handle;
        return NULL;
    }

    // Create DirectSound buffer
    DSBUFFERDESC dsbd = {};
    dsbd.dwSize = sizeof(dsbd);
    dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
    dsbd.dwBufferBytes = 0;
    dsbd.lpwfxFormat = NULL;

    if (FAILED(tv_handle->ds->CreateSoundBuffer(&dsbd, &handle->dsBuffer, NULL)))
    {
        tv_StreamClose(handle);
        return NULL;
    }

    // Configuring DirectSound Buffer
    WAVEFORMATEX wf = {};
    vorbis_info *vi = ov_info(&handle->vorbisFile, -1);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = vi->channels;
    wf.nSamplesPerSec = vi->rate;
    wf.wBitsPerSample = 16; // default 16
    wf.nBlockAlign = wf.nChannels * (wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    if (FAILED(handle->dsBuffer->SetFormat(&wf)))
    {
        tv_StreamClose(handle);
        return NULL;
    }

    return handle;
}

TVPLUGIN_API int tv_StreamLength(TV_AH tv_ah_handle)
{
    return ov_time_total(&tv_ah_handle->vorbisFile, -1);

    /*AudioContext *ctx = g_audioContext;
    fseek(ctx->file, 0, SEEK_END);
    long fileSize = ftell(ctx->file);
    fseek(ctx->file, 0, SEEL_SET);

    return (fileSize / ctx->vi.rate);*/
}

TVPLUGIN_API void tv_StreamPlay(TV_AH tv_ah_handle)
{
    // Read data from vorbisFile & write to DirectSound buffer
    vorbis_info *vi = ov_info(&tv_ah_handle->vorbisFile, -1);
    char buffer[4096];
    int bytesRead;
    while ((bytesRead = ov_read(&tv_ah_handle->vorbisFile, buffer,
        sizeof(buffer), 0, 2, 1, NULL)) > 0)
    {
        DWORD dwWriteOffset = 0;
        void *pDSLockedBuffer = NULL;
        DWORD dwDSLockedBufferSize = 0;

        // Blocking part of DirectSound for write
        if (FAILED(tv_ah_handle->dsBuffer->Lock(dwWriteOffset, bytesRead, &pDSLockedBuffer,
            &dwDSLockedBufferSize, NULL, NULL, 0)))
        {
            break;
        }

        // Copying data Vorbis to DirectSound buffer
        memcpy(pDSLockedBuffer, buffer, dwDSLockedBufferSize);

        // Unlocking DirectSound buffer after write
        tv_ah_handle->dsBuffer->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);

        dwWriteOffset += bytesRead;
    }
    tv_ah_handle->dsBuffer->Play(0, 0, 0);
}


TVPLUGIN_API void tv_StreamClose(TV_AH tv_ah_handle)
{
    ov_clear(&tv_ah_handle->vorbisFile);
    if (tv_ah_handle->dsBuffer)
    {
        tv_ah_handle->dsBuffer->Release();
    }
    free(tv_ah_handle);
}

TVPLUGIN_API BYTE tv_StreamFinished(TV_AH tv_ah_handle)
{
    return ov_time_tell(&tv_ah_handle->vorbisFile) >= ov_time_total(&tv_ah_handle->vorbisFile, -1);
}

TVPLUGIN_API void tv_StreamSetVolume(TV_AH tv_ah_handle, TV_VOLUME tv_volume)
{
    // Setting the volume of the audio stream
    if (tv_ah_handle->dsBuffer)
    {
        // Converting the volume value from the range [0, 100] to the DirectSound range [-10000, 0]
        LONG volume = (tv_volume / 100.0f) * DSBVOLUME_MAX;
        tv_ah_handle->dsBuffer->SetVolume(volume);
    }
    /*if(g_dsbuffer)
    {

        LONG dsVolume = DSBVOLUME_MIN + ((tv_volume - TV_VOLUME_MIN) * (DSBVOLUME_MAX - DSBVOLUME_MIN)) / (TV_VOLUME_MAX - TV_VOLUME_MIN);
        g_dsbuffer->SetVolume(dsVolume);
    }*/
}

TVPLUGIN_API TV_VH tv_VideoOpen(const char *pcszFileName)
{
    TV_VH handle = (TV_VH)malloc(sizeof(TV_HANDLE));
    handle->file = fopen(pcszFileName, "rb");
    if (!handle->file)
    {
        delete handle;
        return NULL;
    }

    // Init Ogg & Theora
    ogg_sync_init(&handle->oggSyncState);
    ogg_stream_init(&handle->theoraStreamState, 0);
    ogg_stream_init(&handle->vorbisStreamState, 0);

    // Read file and init Theora
    char *buffer = ogg_sync_buffer(&handle->oggSyncState, 4096);
    int bytes = fread(buffer, 1, 4096, handle->file);
    ogg_sync_wrote(&handle->oggSyncState, bytes);

    ogg_page oggPage;
    if (ogg_sync_pageout(&handle->oggSyncState, &oggPage) != 1)
    {
        fclose(handle->file);
        delete handle;
        return NULL;
    }


    ogg_stream_pagein(&handle->theoraStreamState, &oggPage);
    ogg_packet oggPacket;

    if (ogg_stream_packetout(&handle->theoraStreamState, &oggPacket) != 1)
    {
        fclose(handle->file);
        delete handle;
        return NULL;
    }

    th_info theoraInfo;
    th_comment theoraComment;
    th_info_init(&theoraInfo);
    th_comment_init(&theoraComment);

    if (th_decode_headerin(&theoraInfo, &theoraComment,
        &handle->theoraSetup, &oggPacket) < 0)
    {
        fclose(handle->file);
        delete handle;
        return NULL;
    }

    handle->theoraDecoder = th_decode_alloc(&theoraInfo, handle->theoraSetup);
    th_info_clear(&theoraInfo);
    th_comment_clear(&theoraComment);

    // Init Direcr3D texture
    if (FAILED(tv_handle->d3dDevice->CreateTexture(
        theoraInfo.frame_width,
        theoraInfo.frame_height,
        1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_X8R8G8B8,
        D3DPOOL_DEFAULT,
        &handle->d3dTexture,
        NULL)))
    {
        fclose(handle->file);
        free(handle);
        return NULL;
    }

    return handle;
}


TVPLUGIN_API int tv_VideoClose(TV_VH tv_vh_handle)
{
    fclose(tv_vh_handle->file);
    ogg_sync_clear(&tv_vh_handle->oggSyncState);
    ogg_stream_clear(&tv_vh_handle->theoraStreamState);
    ogg_stream_clear(&tv_vh_handle->vorbisStreamState);

    if (tv_vh_handle->d3dTexture)
    {
        tv_vh_handle->d3dTexture->Release();
    }
    th_decode_free(tv_vh_handle->theoraDecoder);
    delete tv_vh_handle;

    return TV_OK;
}

TVPLUGIN_API int tv_VideoFinished(TV_VH tv_vh_handle)
{
    return feof(tv_vh_handle->file) ? TV_OK : TV_FAIL;
}

TVPLUGIN_API void tv_VideoPlay(TV_VH tv_vh_handle, BOOL bLooped, RECT *pRect)
{
    while (true)
    {
        ogg_packet oggPacket;
        while (ogg_stream_packetout(&tv_vh_handle->theoraStreamState, &oggPacket) > 0)
        {
            if (th_decode_packetin(tv_vh_handle->theoraDecoder, &oggPacket, NULL) == 0)
            {
                th_ycbcr_buffer ycbcrBuffer;
                th_decode_ycbcr_out(tv_vh_handle->theoraDecoder, ycbcrBuffer);

                // Draw decoding frame on the Direct3D texture 
                D3DLOCKED_RECT lockedRect;
                if (SUCCEEDED(tv_vh_handle->d3dTexture->LockRect(0, &lockedRect, NULL, 0)))
                {
                    for (int y = 0; y < ycbcrBuffer[0].height; y++)
                    {
                        for (int x = 0; x < ycbcrBuffer[0].width; x++)
                        {
                            int y_value = ycbcrBuffer[0].data[y * ycbcrBuffer[0].stride + x];
                            int u_value = ycbcrBuffer[1].data[(y / 2) * ycbcrBuffer[1].stride + (x / 2)];
                            int v_value = ycbcrBuffer[2].data[(y / 2) * ycbcrBuffer[2].stride + (x / 2)];

                            int c = y_value - 16;
                            int d = u_value - 128;
                            int e = v_value - 128;

                            int r = (298 * c + 409 * e + 128) >> 8;
                            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                            int b = (298 * c + 516 * d + 128) >> 8;

                            if (r < 0) r = 0; if (r > 255) r = 255;
                            if (g < 0) g = 0; if (g > 255) g = 255;
                            if (b < 0) b = 0; if (b > 255) b = 255;

                            ((DWORD *)lockedRect.pBits)[y * (lockedRect.Pitch / 4) + x] = D3DCOLOR_XRGB(r, g, b);
                        }
                    }
                    tv_vh_handle->d3dTexture->UnlockRect(0);
                }

                // Rendering  texture on the screen
                tv_vh_handle->d3dDevice->BeginScene();
                tv_vh_handle->d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

                IDirect3DSurface9 *backBuffer = NULL;
                tv_vh_handle->d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);

                if (pRect)
                {
                    // Using pRect for change size of video on the screen
                    RECT srcRect = { 0, 0, ycbcrBuffer[0].width, ycbcrBuffer[0].height };
                    tv_vh_handle->d3dDevice->StretchRect(
                        backBuffer,
                        &srcRect,
                        backBuffer,
                        pRect,
                        D3DTEXF_LINEAR
                    );
                }
                else
                {
                    tv_vh_handle->d3dDevice->StretchRect(
                        backBuffer,
                        NULL,
                        backBuffer,//tv_vh_handle->d3dTexture->GetSurfaceLevel(0, backBuffer), 
                        NULL,
                        D3DTEXF_LINEAR);
                }

                // IDirect3DSurface9 *backBuffer = NULL;
                // tv_vh_handle->d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
                // tv_vh_handle->d3dDevice->StretchRect(backBuffer, NULL, tv_vh_handle->d3dTexture, pRect, D3DTEXF_LINEAR);
                backBuffer->Release();

                tv_vh_handle->d3dDevice->EndScene();
                tv_vh_handle->d3dDevice->Present(NULL, NULL, NULL, NULL);
            }
        }

        // Read the next ogg page
        char *buffer = ogg_sync_buffer(&tv_vh_handle->oggSyncState, 4096);
        int bytes = fread(buffer, 1, 4096, tv_vh_handle->file);
        if (bytes == 0) break;

        ogg_sync_wrote(&tv_vh_handle->oggSyncState, bytes);
        while (ogg_sync_pageout(&tv_vh_handle->oggSyncState, &tv_vh_handle->oggPage) > 0)
        {
            ogg_stream_pagein(&tv_vh_handle->theoraStreamState, &tv_vh_handle->oggPage);
        }

        if (bLooped && feof(tv_vh_handle->file))
        {
            fseek(tv_vh_handle->file, 0, SEEK_SET);
            ogg_sync_reset(&tv_vh_handle->oggSyncState);
            ogg_stream_reset(&tv_vh_handle->theoraStreamState);
            ogg_stream_reset(&tv_vh_handle->vorbisStreamState);
        }
    }
}

TVPLUGIN_API void tv_VideoResize(TV_VH tv_vh_handle, RECT *pRect)
{
    if (pRect)
    {
        // Change size texture for the new window size
        D3DSURFACE_DESC desc;
        tv_vh_handle->d3dTexture->GetLevelDesc(0, &desc);

        if (desc.Width != (pRect->right - pRect->left) || desc.Height != (pRect->bottom - pRect->top))
        {
            // Delete old texture
            if (tv_vh_handle->d3dTexture)
                tv_vh_handle->d3dTexture->Release();
        }

        // Create a new texture with updated size
        tv_vh_handle->d3dDevice->CreateTexture(
            pRect->right - pRect->left,
            pRect->bottom - pRect->top,
            1,
            D3DUSAGE_DYNAMIC,
            D3DFMT_X8R8G8B8,
            D3DPOOL_DEFAULT,
            &tv_vh_handle->d3dTexture,
            NULL
        );

        /*SetWindowPos(g_hwnd, NULL, pRect->left, pRect->top, pRect->right - pRect->left,
            pRect->bottom - pRect->top, SWP_NOZORDER);

        if(ctx->d3dtexture)
            ctx->d3dtexture->Release();

        ctx->d3ddev->CreateTexture(pRect->right - pRect->left, pRect->bottom - pRect->top,
            1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &ctx->d3dtexture, NULL);*/
    }
}