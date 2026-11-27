# 把 assets\tex\morph 下的透明底插画统一成"主体居中、四周留边"的正方精灵。
# 原图是 1536 方图，主体常常顶到画布边缘（图鉴里会被看成被裁掉的方块，
# 展台上又会盖到旁边的名字），这里按 alpha 包围盒裁紧，再等比缩放到
# canvas*fill 以内居中，最后补透明边。全程直通 alpha 双线性，不做预乘，
# 所以半透明的膜、触手、菌丝不会被压黑。
#
# 用法：pwsh -File tools\normalize_sprites.ps1 -Fill 0.86 -Canvas 1536
param(
    [double]$Fill = 0.86,
    [int]$Canvas = 1536,
    [string]$Dir = (Join-Path $PSScriptRoot '..\assets\tex\morph')
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class SpriteNorm
{
    static byte[] Load(string path, out int w, out int h, out int stride)
    {
        using (var fs = File.OpenRead(path))
        using (var bmp = new Bitmap(fs))
        {
            w = bmp.Width; h = bmp.Height;
            var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            stride = data.Stride;
            var buf = new byte[stride * h];
            Marshal.Copy(data.Scan0, buf, 0, buf.Length);
            bmp.UnlockBits(data);
            return buf;
        }
    }

    static void Save(string path, byte[] px, int w, int h, int stride)
    {
        using (var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb))
        {
            var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            var tmp = new byte[data.Stride * h];
            for (int y = 0; y < h; y++) Buffer.BlockCopy(px, y * stride, tmp, y * data.Stride, w * 4);
            Marshal.Copy(tmp, 0, data.Scan0, tmp.Length);
            bmp.UnlockBits(data);
            bmp.Save(path, ImageFormat.Png);
        }
    }

    public static void Run(string dir, double fill, int canvas)
    {
        foreach (var f in Directory.GetFiles(dir, "*.png"))
        {
            int w, h, stride;
            byte[] src = Load(f, out w, out h, out stride);

            int minX = w, minY = h, maxX = -1, maxY = -1;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    if (src[y * stride + x * 4 + 3] > 10)
                    {
                        if (x < minX) minX = x; if (x > maxX) maxX = x;
                        if (y < minY) minY = y; if (y > maxY) maxY = y;
                    }
            if (maxX < 0) continue;

            int bw = maxX - minX + 1, bh = maxY - minY + 1;
            double scale = Math.Min((canvas * fill) / bw, (canvas * fill) / bh);
            int dw = (int)Math.Round(bw * scale), dh = (int)Math.Round(bh * scale);
            int ox = (canvas - dw) / 2, oy = (canvas - dh) / 2;

            var dst = new byte[canvas * canvas * 4];
            for (int y = 0; y < canvas; y++)
            {
                for (int x = 0; x < canvas; x++)
                {
                    int di = (y * canvas + x) * 4;
                    if (x < ox || y < oy || x >= ox + dw || y >= oy + dh) { dst[di + 3] = 0; continue; }

                    double fx = (x - ox + 0.5) / scale + minX - 0.5;
                    double fy = (y - oy + 0.5) / scale + minY - 0.5;
                    if (fx < 0) fx = 0; if (fy < 0) fy = 0;
                    if (fx > w - 1) fx = w - 1; if (fy > h - 1) fy = h - 1;

                    int x0 = (int)Math.Floor(fx), y0 = (int)Math.Floor(fy);
                    int x1 = Math.Min(x0 + 1, w - 1), y1 = Math.Min(y0 + 1, h - 1);
                    double tx = fx - x0, ty = fy - y0;
                    for (int c = 0; c < 4; c++)
                    {
                        double v00 = src[y0 * stride + x0 * 4 + c];
                        double v10 = src[y0 * stride + x1 * 4 + c];
                        double v01 = src[y1 * stride + x0 * 4 + c];
                        double v11 = src[y1 * stride + x1 * 4 + c];
                        double v = (v00 * (1 - tx) + v10 * tx) * (1 - ty) + (v01 * (1 - tx) + v11 * tx) * ty;
                        int iv = (int)Math.Round(v);
                        dst[di + c] = (byte)(iv < 0 ? 0 : (iv > 255 ? 255 : iv));
                    }
                }
            }

            Save(f + ".n.png", dst, canvas, canvas, canvas * 4);
            File.Delete(f);
            File.Move(f + ".n.png", f);
            Console.WriteLine(Path.GetFileName(f) + "  bbox " + bw + "x" + bh + " -> " + dw + "x" + dh);
        }
    }
}
'@

[SpriteNorm]::Run((Resolve-Path $Dir).Path, $Fill, $Canvas)
