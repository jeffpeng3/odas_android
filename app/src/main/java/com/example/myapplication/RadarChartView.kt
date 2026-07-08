package com.example.myapplication

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import kotlin.math.cos
import kotlin.math.sin

data class SoundSource(
    val azimuthDeg: Float,
    val elevationDeg: Float,
    val activity: Float,
    val id: Long,
    val label: String = ""
)

class RadarChartView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val sources = mutableListOf<SoundSource>()

    private val sourceColors = intArrayOf(
        0xFF00BCD4.toInt(),
        0xFFFF5722.toInt(),
        0xFF8BC34A.toInt(),
        0xFFE91E63.toInt(),
        0xFFFFEB3B.toInt(),
        0xFF9C27B0.toInt(),
    )

    private val sectorBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0x0DFFFFFF.toInt()
        style = Paint.Style.FILL
    }
    private val gridPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0x40FFFFFF.toInt()
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0x80FFFFFF.toInt()
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0x80FFFFFF.toInt()
        textSize = 26f
        textAlign = Paint.Align.CENTER
    }
    private val glowFillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }
    private val glowStrokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
    }
    private val dotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }
    private val sourceLabelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 30f
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val arrowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xCCFFFFFF.toInt()
        strokeWidth = 3f
        style = Paint.Style.STROKE
    }
    private val arrowFillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xCCFFFFFF.toInt()
        style = Paint.Style.FILL
    }
    private val arcRect = RectF()
    private val arrowPath = Path()

    private var cx = 0f
    private var cy = 0f
    private var radius = 0f

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        cx = w / 2f
        val size = minOf(w, (h * 0.85f).toInt())
        cy = h * 0.80f
        radius = size * 0.38f
        labelPaint.textSize = radius * 0.08f
        sourceLabelPaint.textSize = radius * 0.095f
    }

    fun updateSources(newSources: List<SoundSource>) {
        sources.clear()
        sources.addAll(newSources)
        postInvalidateOnAnimation()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(0xFF1A1A2E.toInt())
        drawSectorBg(canvas)
        drawGrid(canvas)
        drawForwardMarker(canvas)
        drawSources(canvas)
    }

    private fun drawSectorBg(canvas: Canvas) {
        arcRect.set(cx - radius, cy - radius, cx + radius, cy + radius)
        canvas.drawArc(arcRect, 210f, 120f, true, sectorBgPaint)
    }

    private fun drawGrid(canvas: Canvas) {
        val levels = floatArrayOf(0.25f, 0.5f, 0.75f, 1.0f)
        for (level in levels) {
            val r = radius * level
            arcRect.set(cx - r, cy - r, cx + r, cy + r)
            canvas.drawArc(arcRect, 210f, 120f, false, gridPaint)
        }

        for (az in -60..60 step 15) {
            val rad = Math.toRadians(az.toDouble())
            val ex = cx + radius * sin(rad).toFloat()
            val ey = cy - radius * cos(rad).toFloat()
            canvas.drawLine(cx, cy, ex, ey, gridPaint)

            val lr = radius * 1.10f
            val lx = cx + lr * sin(rad).toFloat()
            val ly = cy - lr * cos(rad).toFloat()
            canvas.drawText("${az}°", lx, ly, labelPaint)
        }

        arcRect.set(cx - radius, cy - radius, cx + radius, cy + radius)
        canvas.drawArc(arcRect, 210f, 120f, false, borderPaint)
    }

    private fun drawForwardMarker(canvas: Canvas) {
        val tipY = cy - radius - radius * 0.12f
        canvas.drawLine(cx, cy - radius, cx, tipY, arrowPaint)
        arrowPath.reset()
        arrowPath.moveTo(cx, tipY - 12f)
        arrowPath.lineTo(cx - 10f, tipY + 4f)
        arrowPath.lineTo(cx + 10f, tipY + 4f)
        arrowPath.close()
        canvas.drawPath(arrowPath, arrowFillPaint)

        labelPaint.color = 0xCCFFFFFF.toInt()
        canvas.drawText("0°", cx, tipY - 16f, labelPaint)
    }

    private fun drawSources(canvas: Canvas) {
        for ((i, src) in sources.withIndex()) {
            val color = sourceColors[i % sourceColors.size]
            val alpha = (src.activity.coerceIn(0f, 1f) * 255).toInt()

            val rad = Math.toRadians(src.azimuthDeg.toDouble())
            val sx = cx + radius * sin(rad).toFloat()
            val sy = cy - radius * cos(rad).toFloat()

            // sector glow
            val glowHalf = 5f
            val ga = (alpha * 0.35f).toInt()
            glowFillPaint.color = Color.argb(ga, Color.red(color), Color.green(color), Color.blue(color))
            arcRect.set(cx - radius, cy - radius, cx + radius, cy + radius)
            canvas.drawArc(arcRect, 270f + src.azimuthDeg - glowHalf, glowHalf * 2f, true, glowFillPaint)

            // stroke glow on outer arc
            val sw = radius * 0.12f
            glowStrokePaint.strokeWidth = sw
            val gsa = (alpha * 0.25f).toInt()
            glowStrokePaint.color = Color.argb(gsa, Color.red(color), Color.green(color), Color.blue(color))
            val sr = radius - sw / 2f
            arcRect.set(cx - sr, cy - sr, cx + sr, cy + sr)
            canvas.drawArc(arcRect, 270f + src.azimuthDeg - glowHalf * 2f, glowHalf * 4f, false, glowStrokePaint)

            // dot
            val dotR = 5f + 12f * src.activity
            dotPaint.color = Color.argb(alpha, Color.red(color), Color.green(color), Color.blue(color))
            canvas.drawCircle(sx, sy, dotR, dotPaint)

            // white inner dot
            dotPaint.color = Color.argb((alpha * 0.6f).toInt(), 0xFF, 0xFF, 0xFF)
            canvas.drawCircle(sx, sy, dotR * 0.35f, dotPaint)

            // label
            sourceLabelPaint.color = Color.argb(alpha, 0xFF, 0xFF, 0xFF)
            canvas.drawText("#${src.id}", sx, sy - dotR - 8f, sourceLabelPaint)
        }
    }
}
