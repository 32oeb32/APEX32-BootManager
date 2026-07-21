#include "Assets/ApexEmblem.hpp"

#include "Renderer/GopRenderer.hpp"

namespace apex32::apexemblem {

void Draw(
    GopRenderer& Renderer,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Size,
    const RgbColor Color) noexcept {
  if ((Size < 12U) || (CenterX < (Size / 2U))) {
    return;
  }

  const UINTN Left = CenterX - (Size / 2U);
  if ((Left > (MAX_UINTN - Size)) || (Top > (MAX_UINTN - Size))) {
    return;
  }
  const UINTN Right = Left + Size;
  const UINTN Bottom = Top + Size;
  const UINTN Half = Size / 2U;
  const UINTN Quarter = Size / 4U;
  const UINTN Thickness = (Size >= 48U) ? (Size / 16U) : 2U;
  const RgbColor Inner = ScaleColor(Color, 150U);

  Renderer.DrawLine(Left + Quarter, Top, Right - Quarter, Top,
                    Thickness, Color);
  Renderer.DrawLine(Right - Quarter, Top, Right, Top + Half,
                    Thickness, Color);
  Renderer.DrawLine(Right, Top + Half, CenterX, Bottom,
                    Thickness, Color);
  Renderer.DrawLine(CenterX, Bottom, Left, Top + Half,
                    Thickness, Color);
  Renderer.DrawLine(Left, Top + Half, Left + Quarter, Top,
                    Thickness, Color);

  Renderer.DrawLine(CenterX, Top + Quarter, Left + Quarter, Bottom - Quarter,
                    Thickness, Inner);
  Renderer.DrawLine(CenterX, Top + Quarter, Right - Quarter, Bottom - Quarter,
                    Thickness, Inner);
  Renderer.DrawLine(Left + (Size / 3U), Top + (Size * 5U / 8U),
                    Right - (Size / 3U), Top + (Size * 5U / 8U),
                    Thickness, Color);
}

}  // namespace apex32::apexemblem
