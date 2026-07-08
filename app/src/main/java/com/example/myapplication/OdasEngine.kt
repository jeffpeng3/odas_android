package com.example.myapplication

class OdasEngine {
    companion object {
        init {
            System.loadLibrary("myapplication")
        }
    }

    external fun initSSL(micSpacingMeters: Float): Boolean
    external fun processAudio(audioData: FloatArray, nChannels: Int): FloatArray
    external fun destroySSL()
}
