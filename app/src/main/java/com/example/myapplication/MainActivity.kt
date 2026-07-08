package com.example.myapplication

import android.Manifest
import android.content.pm.PackageManager
import android.media.AudioDeviceInfo
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.util.Log
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.example.myapplication.databinding.ActivityMainBinding
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val odasEngine = OdasEngine()
    private var processJob: Job? = null
    private var audioRecord: AudioRecord? = null

    companion object {
        const val SAMPLE_RATE = 16000
        const val HOP_SIZE = 128
        const val N_CHANNELS = 2
        const val MIC_SPACING = 0.08f
        const val SOUND_SPEED = 343f
    }

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) startCapture()
            else {
                binding.tvMicInfo.text = "RECORD_AUDIO permission denied"
                Log.e("ODAS_DEMO", "Permission denied")
            }
        }

    // Identify built-in (non-BT) input devices for display
    private data class MicInfo(val id: Int, val name: String, val address: String, val channels: String, val rates: String)

    private val builtInMics = mutableListOf<MicInfo>()

    private fun buildMicInfo(): String {
        val mgr = getSystemService(AUDIO_SERVICE) as AudioManager
        val devices = mgr.getDevices(AudioManager.GET_DEVICES_INPUTS)
        val sb = StringBuilder()
        builtInMics.clear()
        val btTypes = setOf(AudioDeviceInfo.TYPE_BLUETOOTH_SCO, AudioDeviceInfo.TYPE_BLUETOOTH_A2DP)
        val local = devices.filter { it.type !in btTypes }
        sb.appendLine("=== Local Input Devices (${local.size}) ===")
        local.forEachIndexed { i, d ->
            val type = when (d.type) {
                AudioDeviceInfo.TYPE_BUILTIN_MIC -> "BUILTIN_MIC"
                AudioDeviceInfo.TYPE_USB_DEVICE -> "USB_DEVICE"
                AudioDeviceInfo.TYPE_USB_HEADSET -> "USB_HEADSET"
                AudioDeviceInfo.TYPE_WIRED_HEADSET -> "WIRED_HEADSET"
                AudioDeviceInfo.TYPE_TELEPHONY -> "TELEPHONY"
                AudioDeviceInfo.TYPE_FM_TUNER -> "FM_TUNER"
                AudioDeviceInfo.TYPE_HDMI -> "HDMI"
                AudioDeviceInfo.TYPE_LINE_ANALOG -> "LINE_ANALOG"
                AudioDeviceInfo.TYPE_LINE_DIGITAL -> "LINE_DIGITAL"
                AudioDeviceInfo.TYPE_REMOTE_SUBMIX -> "REMOTE_SUBMIX"
                AudioDeviceInfo.TYPE_USB_ACCESSORY -> "USB_ACCESSORY"
                else -> "OTHER(${d.type})"
            }
            val addr = d.address.ifEmpty { "N/A" }
            val chs = d.channelCounts?.joinToString(", ") ?: "N/A"
            val rates = d.sampleRates?.joinToString(", ") ?: "N/A"
            sb.appendLine("#${i} ${d.productName}  [$type]")
            sb.appendLine("    addr=$addr  ch=$chs  rates=$rates")
            if (d.type == AudioDeviceInfo.TYPE_BUILTIN_MIC) {
                builtInMics.add(MicInfo(d.id, d.productName.toString(), addr, chs, rates))
            }
        }
        sb.appendLine("=== Capture Config ===")
        sb.appendLine("    Sample Rate: $SAMPLE_RATE  Channels: $N_CHANNELS")
        sb.appendLine("    Mic Spacing: ${(MIC_SPACING * 100).toInt()} cm (estimated)")
        return sb.toString()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.tvMicInfo.text = buildMicInfo()
        binding.radarView.updateSources(listOf(SoundSource(0f, 0f, 0f, 0L)))

        when {
            ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED -> {
                startCapture()
            }
            shouldShowRequestPermissionRationale(Manifest.permission.RECORD_AUDIO) -> {
                binding.tvMicInfo.text = buildMicInfo() + "\nMic permission needed for capture"
                requestPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
            }
            else -> {
                requestPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
            }
        }
    }

    private fun updateRmsBars(rms0: Float, rms1: Float) {
        val panel = binding.liveRmsPanel
        if (panel.width == 0) return
        val maxW = (panel.width / 2) - 40  // half panel minus padding
        val scale = 0.3f  // expected max RMS
        val w0 = ((rms0 / scale) * maxW).toInt().coerceIn(0, maxW)
        val w1 = ((rms1 / scale) * maxW).toInt().coerceIn(0, maxW)
        binding.barCh0.layoutParams.width = w0
        binding.barCh1.layoutParams.width = w1
        binding.barCh0.requestLayout()
        binding.barCh1.requestLayout()
        binding.tvCh0Rms.text = "%.4f".format(rms0)
        binding.tvCh1Rms.text = "%.4f".format(rms1)
    }

    private fun startCapture() {
        val ok = odasEngine.initSSL(MIC_SPACING)
        if (!ok) {
            Log.e("ODAS_DEMO", "SSL init failed")
            return
        }
        Log.i("ODAS_DEMO", "SSL initialized, spacing=${MIC_SPACING}m")

        // Try CAMCORDER stereo first
        val channelMask = AudioFormat.CHANNEL_IN_STEREO
        val minBuf = AudioRecord.getMinBufferSize(SAMPLE_RATE, channelMask, AudioFormat.ENCODING_PCM_16BIT)
        if (minBuf == AudioRecord.ERROR || minBuf == AudioRecord.ERROR_BAD_VALUE) {
            Log.w("ODAS_DEMO", "CAMCORDER stereo not supported, trying MONO")
            startCaptureMono()
            return
        }

        try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.CAMCORDER,
                SAMPLE_RATE,
                channelMask,
                AudioFormat.ENCODING_PCM_16BIT,
                minBuf * 4
            )
        } catch (e: Exception) {
            Log.e("ODAS_DEMO", "AudioRecord init failed: ${e.message}")
            startCaptureMono()
            return
        }

        val ar = audioRecord!!
        if (ar.state != AudioRecord.STATE_INITIALIZED) {
            Log.w("ODAS_DEMO", "AudioRecord not initialized, trying MONO")
            ar.release()
            startCaptureMono()
            return
        }

        val captureChannels = ar.channelCount
        Log.i("ODAS_DEMO", "AudioRecord OK: rate=${ar.sampleRate} ch=$captureChannels buf=$minBuf")
        // Use AudioManager to show which input devices are available
        val mgr = getSystemService(AUDIO_SERVICE) as AudioManager
        val inputDevices = mgr.getDevices(AudioManager.GET_DEVICES_INPUTS)
        for (d in inputDevices) {
            Log.i("ODAS_DEMO", "Input device: id=${d.id} product='${d.productName}' type=${d.type} addr='${d.address}'")
        }

        binding.tvMicInfo.text = buildMicInfo() +
            "\n=== Live Capture ===" +
            "\n    Status: ACTIVE (${captureChannels}ch)" +
            "\n    Buffer: ${minBuf * 4} bytes"

        // Physical positions (HAL addr → actual location):
        //   "bottom" → near camera lens
        //   "back"   → near USB-C charging port
        val pos0 = if (builtInMics.size > 0) when (builtInMics[0].address) {
            "bottom" -> "camera"; "back" -> "usb-c"; else -> builtInMics[0].address
        } else "?"
        val pos1 = if (builtInMics.size > 1) when (builtInMics[1].address) {
            "bottom" -> "camera"; "back" -> "usb-c"; else -> builtInMics[1].address
        } else "?"
        binding.tvCh0Label.text = "ch0 ($pos0)"
        binding.tvCh1Label.text = "ch1 ($pos1)"

        ar.startRecording()
        val shortBuf = ShortArray(HOP_SIZE * captureChannels)
        val floatBuf = FloatArray(HOP_SIZE * captureChannels)

        processJob = CoroutineScope(Dispatchers.Default).launch {
            var frameCount = 0
            var rms0 = 0f; var rms1 = 0f
            while (isActive) {
                val read = ar.read(shortBuf, 0, shortBuf.size, AudioRecord.READ_BLOCKING)
                if (read == shortBuf.size) {
                    for (i in 0 until read) {
                        floatBuf[i] = shortBuf[i].toFloat() / 32768f
                    }
                    // Per-channel RMS
                    rms0 = 0f; rms1 = 0f
                    for (i in 0 until HOP_SIZE) {
                        rms0 += floatBuf[i * 2] * floatBuf[i * 2]
                        rms1 += floatBuf[i * 2 + 1] * floatBuf[i * 2 + 1]
                    }
                    rms0 = kotlin.math.sqrt(rms0 / HOP_SIZE)
                    rms1 = kotlin.math.sqrt(rms1 / HOP_SIZE)

                    val results = odasEngine.processAudio(floatBuf, captureChannels)
                    val directAz = results[0]
                    val quality = results[1]

                    frameCount++
                    // Update UI every 4 frames (~32ms) to avoid overloading main thread
                    if (frameCount % 4 == 0) {
                        val finalAz = directAz
                        val finalQ = quality
                        val fr0 = rms0; val fr1 = rms1
                        withContext(Dispatchers.Main) {
                            binding.radarView.updateSources(
                                listOf(SoundSource(finalAz, 0f, finalQ.coerceIn(0f, 1f), 1L, "S1"))
                            )
                            updateRmsBars(fr0, fr1)
                        }
                    }
                } else if (read == AudioRecord.ERROR_INVALID_OPERATION) {
                    Log.e("ODAS_DEMO", "AudioRecord invalid operation")
                    break
                } else {
                    delay(1)
                }
            }
        }
    }

    private fun startCaptureMono() {
        val channelMask = AudioFormat.CHANNEL_IN_MONO
        val minBuf = AudioRecord.getMinBufferSize(SAMPLE_RATE, channelMask, AudioFormat.ENCODING_PCM_16BIT)
        if (minBuf == AudioRecord.ERROR || minBuf == AudioRecord.ERROR_BAD_VALUE) {
            Log.e("ODAS_DEMO", "MONO not supported either")
            binding.tvMicInfo.text = buildMicInfo() + "\nNo supported capture config"
            return
        }

        try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.DEFAULT,
                SAMPLE_RATE,
                channelMask,
                AudioFormat.ENCODING_PCM_16BIT,
                minBuf * 4
            )
        } catch (e: Exception) {
            Log.e("ODAS_DEMO", "MONO AudioRecord failed: ${e.message}")
            binding.tvMicInfo.text = buildMicInfo() + "\nCapture init failed"
            return
        }

        val ar = audioRecord!!
        if (ar.state != AudioRecord.STATE_INITIALIZED) {
            Log.e("ODAS_DEMO", "MONO AudioRecord not initialized")
            ar.release()
            audioRecord = null
            return
        }

        Log.i("ODAS_DEMO", "MONO AudioRecord OK, rate=${ar.sampleRate}")
        binding.tvMicInfo.text = buildMicInfo() +
            "\n=== Live Capture ===" +
            "\n    Status: ACTIVE (MONO - 1ch only)" +
            "\n    Note: Stereo unsupported on this device"

        ar.startRecording()
        val shortBuf = ShortArray(HOP_SIZE)
        val floatBuf = FloatArray(HOP_SIZE * 2) // need 2 channels for ODAS

        processJob = CoroutineScope(Dispatchers.Default).launch {
            while (isActive) {
                val read = ar.read(shortBuf, 0, shortBuf.size, AudioRecord.READ_BLOCKING)
                if (read == shortBuf.size) {
                    // Duplicate mono to both channels for ODAS 2-ch input
                    for (i in 0 until HOP_SIZE) {
                        val f = shortBuf[i].toFloat() / 32768f
                        floatBuf[i * 2] = f
                        floatBuf[i * 2 + 1] = f
                    }
                    val results = odasEngine.processAudio(floatBuf, 2)
                    val quality = results[1]
                    withContext(Dispatchers.Main) {
                        binding.radarView.updateSources(
                            listOf(SoundSource(results[0], 0f, quality.coerceIn(0f, 1f), 1L, "S1"))
                        )
                    }
                } else if (read == AudioRecord.ERROR_INVALID_OPERATION) {
                    break
                }
            }
        }
    }

    override fun onDestroy() {
        processJob?.cancel()
        audioRecord?.let {
            if (it.recordingState == AudioRecord.RECORDSTATE_RECORDING) it.stop()
            it.release()
        }
        odasEngine.destroySSL()
        super.onDestroy()
    }
}
