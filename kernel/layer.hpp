#pragma once

#include <memory>
#include <map>
#include <vector>

#include "graphics.hpp"
#include "window.hpp"
#include "message.hpp"

// 原点の座標と重なり順のみを保持する
class Layer {
 public:
  Layer(unsigned int id = 0);
  unsigned int ID() const;

  Layer& SetWindow(const std::shared_ptr<Window>& window);
  std::shared_ptr<Window> GetWindow() const;
  Vector2D<int> GetPosition() const;
  Layer& SetDraggable(bool draggable);
  bool IsDraggable() const;
  // 絶対座標に原ベースとなる点を移動させる
  Layer& Move(Vector2D<int> pos);
  Layer& MoveRelative(Vector2D<int> pos_diff);
  void DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const;

 private:
  unsigned int id_;
  Vector2D<int> pos_{};
  std::shared_ptr<Window> window_{};
  bool draggable_{false};
};

class LayerManager {
 public:
  void SetWriter(FrameBuffer* screen);
  Layer& NewLayer();
  void Draw(const Rectangle<int>& area) const;
  // 指定したレイヤーに設定されているウィンドウの描画領域内を再描画する。
  void Draw(unsigned int id) const;
  void Draw(unsigned int id, Rectangle<int> area) const;
  void Move(unsigned int id, Vector2D<int> new_pos);
  void MoveRelative(unsigned int id, Vector2D<int> pos_diff);
  void UpDown(unsigned int id, int new_height);
  void Hide(unsigned int id);
  Layer* FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const;
  Layer* FindLayer(unsigned int id);
  int GetHeight(unsigned int id); // その ID のレイヤが表示順 ID を格納している layer_stack_ の中で何番目に高いかを返す。

 private:
  FrameBuffer* screen_{nullptr}; // シャドウバッファを格納する変数
  mutable FrameBuffer back_buffer_{};
  std::vector<std::unique_ptr<Layer>> layers_{};
  std::vector<Layer*> layer_stack_{};
  unsigned int latest_id_{0};
};

extern LayerManager* layer_manager;

class ActiveLayer {
 public:
  ActiveLayer(LayerManager& manager);
  void SetMouseLayer(unsigned int mouse_layer);
  void Activate(unsigned int layer_id);
  unsigned int GetActive() const { return active_layer_; }

 private:
  LayerManager& manager_;
  unsigned int active_layer_{0};
  unsigned int mouse_layer_{0};
};

extern ActiveLayer* active_layer;
extern std::map<unsigned int, uint64_t>* layer_task_map; // layer と task の関連性を保持した変数

void InitializeLayer();
void ProcessLayerMessage(const Message& msg);

constexpr Message MakeLayerMessage(
  uint64_t task_id, unsigned int layer_id,
  LayerOperation op, const Rectangle<int>& area) {
  Message msg{Message::kLayer, task_id};
  msg.arg.layer.layer_id = layer_id;
  msg.arg.layer.op = op;
  msg.arg.layer.x = area.pos.x;
  msg.arg.layer.y = area.pos.y;
  msg.arg.layer.w = area.size.x;
  msg.arg.layer.h = area.size.y;
  return msg;
}
