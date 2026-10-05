#pragma once

#include <string>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text/text.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::qr701 {

class QR701PrintText;

class QR701 : public PollingComponent, public uart::UARTDevice {
 public:
  void set_print_text(QR701PrintText *print_text) { this->print_text_ = print_text; }
  void set_status_text_sensor(text_sensor::TextSensor *status) { this->status_ = status; }
  void set_paper_out_binary_sensor(binary_sensor::BinarySensor *paper_out) { this->paper_out_sensor_ = paper_out; }
  void set_cover_open_binary_sensor(binary_sensor::BinarySensor *cover_open) { this->cover_open_sensor_ = cover_open; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *error) { this->error_sensor_ = error; }

  void print(const std::string &text) {
    // ESC/POS requires a status reply to be read before additional data is
    // sent. A poll takes at most 400 ms, so queue one action arriving in that
    // narrow window rather than corrupting either transaction.
    if (this->awaiting_status_) {
      this->queued_text_ = text;
      this->queued_markdown_ = false;
      this->print_queued_ = true;
      this->publish_status_("printing");
      return;
    }
    this->start_print_(text);
  }

  void print_markdown(const std::string &markdown) {
    if (this->awaiting_status_) {
      this->queued_text_ = markdown;
      this->queued_markdown_ = true;
      this->print_queued_ = true;
      this->publish_status_("printing");
      return;
    }
    this->start_markdown_print_(markdown);
  }

  void feed(uint8_t lines) {
    if (this->awaiting_status_) {
      this->queued_feed_lines_ = lines;
      this->feed_queued_ = true;
      this->publish_status_("printing");
      return;
    }
    this->start_feed_(lines);
  }

  void refresh_status();
  void print_text_field();

  void update() override;
  void loop() override;
  void dump_config() override;

 protected:
  void start_print_(const std::string &text) {
    // ESC/POS-compatible printers accept printable bytes directly. Use the
    // string length rather than write_str() so embedded newlines are retained.
    this->write_array(reinterpret_cast<const uint8_t *>(text.data()), text.size());

    // Terminate the final text line and advance the receipt far enough to tear.
    static constexpr uint8_t FEED[] = {'\n', '\n', '\n'};
    this->write_array(FEED, sizeof(FEED));

    // ESC/POS status has no portable "mechanism currently printing" bit.
    // Keep a local state briefly, then verify the printer's fault state.
    uint32_t line_count = 1;
    for (const char character : text) {
      if (character == '\n')
        line_count++;
    }
    this->printing_until_ = millis() + 250 + line_count * 100;
    this->printing_ = true;
    this->publish_status_("printing");
  }

  void start_feed_(uint8_t lines) {
    // ESC d n: print the current line and feed n lines.
    this->write_byte(0x1B);
    this->write_byte(0x64);
    this->write_byte(lines);
    this->printing_until_ = millis() + 250 + static_cast<uint32_t>(lines) * 100;
    this->printing_ = true;
    this->publish_status_("printing");
  }

  void start_markdown_print_(const std::string &markdown);

  void request_status_(uint8_t query);
  void process_status_(uint8_t status);
  void publish_binary_status_();
  void publish_printer_status_();
  void publish_status_(const char *status);
  bool status_enabled_() const;

  text_sensor::TextSensor *status_{nullptr};
  binary_sensor::BinarySensor *paper_out_sensor_{nullptr};
  binary_sensor::BinarySensor *cover_open_sensor_{nullptr};
  binary_sensor::BinarySensor *error_sensor_{nullptr};
  QR701PrintText *print_text_{nullptr};
  uint8_t query_{0};
  uint32_t query_started_at_{0};
  uint32_t printing_until_{0};
  std::string queued_text_;
  uint8_t queued_feed_lines_{0};
  bool awaiting_status_{false};
  bool printing_{false};
  bool print_queued_{false};
  bool queued_markdown_{false};
  bool feed_queued_{false};
  bool offline_{false};
  bool cover_open_{false};
  bool paper_out_{false};
  bool error_{false};
};

// A Home Assistant text entity that holds the next receipt until the matching
// QR701PrintButton is pressed.
class QR701PrintText : public text::Text {
 protected:
  void control(const std::string &value) override { this->publish_state(value); }
};

class QR701PrintButton : public button::Button {
 public:
  void set_parent(QR701 *parent) { this->parent_ = parent; }

 protected:
  void press_action() override {
    if (this->parent_ != nullptr)
      this->parent_->print_text_field();
  }

  QR701 *parent_{nullptr};
};

template<typename... Ts> class QR701PrintAction : public Action<Ts...> {
 public:
  explicit QR701PrintAction(QR701 *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, text)

  void play(const Ts &...x) override { this->parent_->print(this->text_.value(x...)); }

 protected:
  QR701 *parent_;
};

template<typename... Ts> class QR701MarkdownPrintAction : public Action<Ts...> {
 public:
  explicit QR701MarkdownPrintAction(QR701 *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, text)

  void play(const Ts &...x) override { this->parent_->print_markdown(this->text_.value(x...)); }

 protected:
  QR701 *parent_;
};

template<typename... Ts> class QR701FeedAction : public Action<Ts...> {
 public:
  explicit QR701FeedAction(QR701 *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint8_t, lines)

  void play(const Ts &...x) override { this->parent_->feed(this->lines_.value(x...)); }

 protected:
  QR701 *parent_;
};

template<typename... Ts> class QR701RefreshStatusAction : public Action<Ts...> {
 public:
  explicit QR701RefreshStatusAction(QR701 *parent) : parent_(parent) {}

  void play(const Ts &...) override { this->parent_->refresh_status(); }

 protected:
  QR701 *parent_;
};

}  // namespace esphome::qr701
