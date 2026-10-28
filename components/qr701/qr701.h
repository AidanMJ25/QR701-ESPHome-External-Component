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
class QR701QRData;
class QR701BarcodeData;

class QR701 : public PollingComponent, public uart::UARTDevice {
 public:
  void set_print_text(QR701PrintText *print_text) { this->print_text_ = print_text; }
  void set_qr_data(QR701QRData *qr_data) { this->qr_data_ = qr_data; }
  void set_barcode_data(QR701BarcodeData *barcode_data) { this->barcode_data_ = barcode_data; }
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
      this->queued_job_ = QueuedJob::TEXT;
      this->publish_status_("printing");
      return;
    }
    this->start_print_(text);
  }

  void print_markdown(const std::string &markdown) {
    if (this->awaiting_status_) {
      this->queued_text_ = markdown;
      this->queued_job_ = QueuedJob::MARKDOWN;
      this->publish_status_("printing");
      return;
    }
    this->start_markdown_print_(markdown);
  }

  void print_qr(const std::string &text, uint8_t size, uint8_t error_correction) {
    if (this->awaiting_status_) {
      this->queued_text_ = text;
      this->queued_qr_size_ = size;
      this->queued_qr_error_correction_ = error_correction;
      this->queued_job_ = QueuedJob::QR;
      this->publish_status_("printing");
      return;
    }
    this->start_qr_print_(text, size, error_correction);
  }

  void print_barcode(const std::string &text, uint8_t format, uint8_t height, uint8_t width, bool show_text) {
    if (this->awaiting_status_) {
      this->queued_text_ = text;
      this->queued_barcode_format_ = format;
      this->queued_barcode_height_ = height;
      this->queued_barcode_width_ = width;
      this->queued_barcode_show_text_ = show_text;
      this->queued_job_ = QueuedJob::BARCODE;
      this->publish_status_("printing");
      return;
    }
    this->start_barcode_print_(text, format, height, width, show_text);
  }

  void feed(uint8_t lines) {
    if (this->awaiting_status_) {
      this->queued_feed_lines_ = lines;
      this->queued_job_ = QueuedJob::FEED;
      this->publish_status_("printing");
      return;
    }
    this->start_feed_(lines);
  }

  void refresh_status();
  void print_text_field();
  void print_markdown_text_field();
  void print_qr_data_field();
  void print_barcode_data_field();

  void update() override;
  void loop() override;
  void dump_config() override;

 protected:
  enum class QueuedJob : uint8_t { NONE, TEXT, MARKDOWN, FEED, QR, BARCODE };

  void start_print_(const std::string &text);
  void start_qr_print_(const std::string &text, uint8_t size, uint8_t error_correction);
  void start_barcode_print_(const std::string &text, uint8_t format, uint8_t height, uint8_t width, bool show_text);
  void run_queued_job_();

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
  QR701QRData *qr_data_{nullptr};
  QR701BarcodeData *barcode_data_{nullptr};
  uint8_t query_{0};
  uint32_t query_started_at_{0};
  uint32_t printing_until_{0};
  std::string queued_text_;
  uint8_t queued_feed_lines_{0};
  uint8_t queued_qr_size_{5};
  uint8_t queued_qr_error_correction_{49};
  uint8_t queued_barcode_format_{73};
  uint8_t queued_barcode_height_{80};
  uint8_t queued_barcode_width_{2};
  bool queued_barcode_show_text_{true};
  QueuedJob queued_job_{QueuedJob::NONE};
  bool awaiting_status_{false};
  bool printing_{false};
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

class QR701QRData : public text::Text {
 protected:
  void control(const std::string &value) override { this->publish_state(value); }
};

class QR701BarcodeData : public text::Text {
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

class QR701MarkdownPrintButton : public button::Button {
 public:
  void set_parent(QR701 *parent) { this->parent_ = parent; }

 protected:
  void press_action() override {
    if (this->parent_ != nullptr)
      this->parent_->print_markdown_text_field();
  }

  QR701 *parent_{nullptr};
};

class QR701PrintQRButton : public button::Button {
 public:
  void set_parent(QR701 *parent) { this->parent_ = parent; }

 protected:
  void press_action() override {
    if (this->parent_ != nullptr)
      this->parent_->print_qr_data_field();
  }

  QR701 *parent_{nullptr};
};

class QR701PrintBarcodeButton : public button::Button {
 public:
  void set_parent(QR701 *parent) { this->parent_ = parent; }

 protected:
  void press_action() override {
    if (this->parent_ != nullptr)
      this->parent_->print_barcode_data_field();
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

  void set_has_text(bool has_text) { this->has_text_ = has_text; }

  void play(const Ts &...x) override {
    if (this->has_text_)
      this->parent_->print_markdown(this->text_.value(x...));
    else
      this->parent_->print_markdown_text_field();
  }

 protected:
  QR701 *parent_;
  bool has_text_{false};
};

template<typename... Ts> class QR701PrintQRAction : public Action<Ts...> {
 public:
  explicit QR701PrintQRAction(QR701 *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, text)
  TEMPLATABLE_VALUE(uint8_t, size)
  TEMPLATABLE_VALUE(uint8_t, error_correction)

  void play(const Ts &...x) override {
    this->parent_->print_qr(this->text_.value(x...), this->size_.value(x...), this->error_correction_.value(x...));
  }

 protected:
  QR701 *parent_;
};

template<typename... Ts> class QR701PrintBarcodeAction : public Action<Ts...> {
 public:
  explicit QR701PrintBarcodeAction(QR701 *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, text)
  TEMPLATABLE_VALUE(uint8_t, format)
  TEMPLATABLE_VALUE(uint8_t, height)
  TEMPLATABLE_VALUE(uint8_t, width)
  TEMPLATABLE_VALUE(bool, show_text)

  void play(const Ts &...x) override {
    this->parent_->print_barcode(this->text_.value(x...), this->format_.value(x...), this->height_.value(x...),
                                 this->width_.value(x...), this->show_text_.value(x...));
  }

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
