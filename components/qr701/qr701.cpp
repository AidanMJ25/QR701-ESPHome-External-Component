#include "qr701.h"

#include "esphome/core/log.h"

namespace esphome::qr701 {

static const char *const TAG = "qr701";

namespace {

void write_escape(QR701 *printer, uint8_t command, uint8_t value) {
  printer->write_byte(0x1B);
  printer->write_byte(command);
  printer->write_byte(value);
}

void write_italic(QR701 *printer, bool enabled) {
  // ESC 4 enables italic and ESC 5 disables it; unlike ESC E and ESC -, these
  // commands do not take a parameter byte.
  printer->write_byte(0x1B);
  printer->write_byte(enabled ? 0x34 : 0x35);
}

void write_inline_markdown(QR701 *printer, const std::string &text) {
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool code = false;

  for (size_t index = 0; index < text.size();) {
    if (text[index] == '\\' && index + 1 < text.size()) {
      printer->write_byte(static_cast<uint8_t>(text[index + 1]));
      index += 2;
    } else if (index + 1 < text.size() &&
               ((text[index] == '*' && text[index + 1] == '*') ||
                (text[index] == '_' && text[index + 1] == '_'))) {
      bold = !bold;
      write_escape(printer, 0x45, bold ? 1 : 0);  // ESC E: emphasis.
      index += 2;
    } else if (index + 1 < text.size() && text[index] == '~' && text[index + 1] == '~') {
      // ESC/POS has no portable strikethrough; underline is the closest
      // broadly supported receipt-printer equivalent.
      underline = !underline;
      write_escape(printer, 0x2D, underline ? 1 : 0);  // ESC -: underline.
      index += 2;
    } else if (text[index] == '*' || text[index] == '_') {
      italic = !italic;
      write_italic(printer, italic);
      index++;
    } else if (text[index] == '`') {
      code = !code;
      write_escape(printer, 0x4D, code ? 1 : 0);  // ESC M: Font B / Font A.
      index++;
    } else if (text[index] == '[') {
      const size_t close_label = text.find("](", index + 1);
      const size_t close_url = close_label == std::string::npos ? std::string::npos : text.find(')', close_label + 2);
      if (close_url != std::string::npos) {
        write_inline_markdown(printer, text.substr(index + 1, close_label - index - 1));
        const std::string url = text.substr(close_label + 2, close_url - close_label - 2);
        printer->write_byte(' ');
        printer->write_byte('(');
        printer->write_array(reinterpret_cast<const uint8_t *>(url.data()), url.size());
        printer->write_byte(')');
        index = close_url + 1;
      } else {
        printer->write_byte(static_cast<uint8_t>(text[index++]));
      }
    } else {
      printer->write_byte(static_cast<uint8_t>(text[index++]));
    }
  }

  // Do not allow an incomplete Markdown marker to leak formatting into the
  // following line or a later print job.
  write_escape(printer, 0x45, 0);
  write_italic(printer, false);
  write_escape(printer, 0x2D, 0);
  write_escape(printer, 0x4D, 0);
}

bool is_horizontal_rule(const std::string &line) {
  if (line.size() < 3)
    return false;
  const char marker = line[0];
  if (marker != '-' && marker != '*' && marker != '_')
    return false;
  for (const char character : line) {
    if (character != marker)
      return false;
  }
  return true;
}

}  // namespace

void QR701::print_text_field() {
  if (this->print_text_ == nullptr) {
    ESP_LOGW(TAG, "Print button pressed without a text entity");
    return;
  }
  this->print_markdown(this->print_text_->state);
}

void QR701::start_markdown_print_(const std::string &markdown) {
  size_t line_start = 0;
  uint32_t line_count = 0;

  while (line_start <= markdown.size()) {
    const size_t line_end = markdown.find('\n', line_start);
    std::string line = markdown.substr(line_start, line_end - line_start);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (is_horizontal_rule(line)) {
      static constexpr char RULE[] = "--------------------------------";
      this->write_array(reinterpret_cast<const uint8_t *>(RULE), sizeof(RULE) - 1);
    } else {
      size_t content_start = 0;
      uint8_t heading_level = 0;
      while (content_start < line.size() && line[content_start] == '#' && heading_level < 3) {
        heading_level++;
        content_start++;
      }
      if (heading_level > 0 && content_start < line.size() && line[content_start] == ' ') {
        while (content_start < line.size() && line[content_start] == ' ')
          content_start++;
        write_escape(this, 0x61, 1);  // ESC a: centre headings.
        write_escape(this, 0x21, heading_level == 1 ? 0x30 : 0x10);
        write_escape(this, 0x45, 1);
        write_inline_markdown(this, line.substr(content_start));
        write_escape(this, 0x21, 0);
        write_escape(this, 0x61, 0);
      } else if (line.rfind("> ", 0) == 0) {
        this->write_array(reinterpret_cast<const uint8_t *>("| "), 2);
        write_inline_markdown(this, line.substr(2));
      } else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0 || line.rfind("+ ", 0) == 0) {
        this->write_array(reinterpret_cast<const uint8_t *>("- "), 2);
        write_inline_markdown(this, line.substr(2));
      } else {
        write_inline_markdown(this, line);
      }
    }

    this->write_byte('\n');
    line_count++;
    if (line_end == std::string::npos)
      break;
    line_start = line_end + 1;
  }

  static constexpr uint8_t FEED[] = {'\n', '\n', '\n'};
  this->write_array(FEED, sizeof(FEED));
  this->printing_until_ = millis() + 250 + line_count * 100;
  this->printing_ = true;
  this->publish_status_("printing");
}

void QR701::update() {
  if (!this->status_enabled_() || this->awaiting_status_)
    return;
  this->refresh_status();
}

void QR701::refresh_status() {
  if (this->awaiting_status_)
    return;
  this->offline_ = false;
  this->cover_open_ = false;
  this->paper_out_ = false;
  this->error_ = false;
  this->request_status_(1);
}

void QR701::loop() {
  if (!this->awaiting_status_)
    return;

  uint8_t response;
  if (this->read_byte(&response)) {
    this->awaiting_status_ = false;
    this->process_status_(response);
    if (this->query_ < 4) {
      this->request_status_(this->query_ + 1);
    } else {
      this->publish_printer_status_();
      if (this->feed_queued_) {
        this->feed_queued_ = false;
        this->start_feed_(this->queued_feed_lines_);
      }
      if (this->print_queued_) {
        this->print_queued_ = false;
        if (this->queued_markdown_)
          this->start_markdown_print_(this->queued_text_);
        else
          this->start_print_(this->queued_text_);
        this->queued_markdown_ = false;
        this->queued_text_.clear();
      }
    }
    return;
  }

  if (millis() - this->query_started_at_ > 100) {
    this->awaiting_status_ = false;
    this->publish_status_("unavailable");
    if (this->feed_queued_) {
      this->feed_queued_ = false;
      this->start_feed_(this->queued_feed_lines_);
    }
    if (this->print_queued_) {
      this->print_queued_ = false;
      if (this->queued_markdown_)
        this->start_markdown_print_(this->queued_text_);
      else
        this->start_print_(this->queued_text_);
      this->queued_markdown_ = false;
      this->queued_text_.clear();
    }
  }
}

void QR701::request_status_(uint8_t query) {
  this->query_ = query;
  this->write_byte(0x10);  // DLE
  this->write_byte(0x04);  // EOT
  this->write_byte(query);
  this->query_started_at_ = millis();
  this->awaiting_status_ = true;
}

void QR701::process_status_(uint8_t status) {
  // ESC/POS DLE EOT n real-time responses, n = 1..4.
  switch (this->query_) {
    case 1:
      this->offline_ = (status & 0x08) != 0;
      break;
    case 2:
      this->cover_open_ = (status & 0x04) != 0;
      this->paper_out_ = (status & 0x20) != 0;
      this->error_ = (status & 0x40) != 0;
      break;
    case 3:
      // DLE EOT 3: bits 2, 3, 5, and 6 report recoverable, cutter,
      // unrecoverable, and auto-recoverable errors respectively.
      this->error_ = this->error_ || (status & 0x6C) != 0;
      break;
    case 4:
      this->paper_out_ = this->paper_out_ || (status & 0x60) != 0;
      break;
  }
}

void QR701::publish_printer_status_() {
  this->publish_binary_status_();
  if (this->error_) {
    this->publish_status_("error");
  } else if (this->paper_out_) {
    this->publish_status_("paper_out");
  } else if (this->cover_open_) {
    this->publish_status_("cover_open");
  } else if (this->offline_) {
    this->publish_status_("offline");
  } else if (this->printing_ && millis() < this->printing_until_) {
    this->publish_status_("printing");
  } else {
    this->printing_ = false;
    this->publish_status_("idle");
  }
}

void QR701::publish_binary_status_() {
  if (this->paper_out_sensor_ != nullptr)
    this->paper_out_sensor_->publish_state(this->paper_out_);
  if (this->cover_open_sensor_ != nullptr)
    this->cover_open_sensor_->publish_state(this->cover_open_);
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(this->error_);
}

void QR701::publish_status_(const char *status) {
  ESP_LOGD(TAG, "Printer status: %s", status);
  if (this->status_ != nullptr)
    this->status_->publish_state(status);
}

bool QR701::status_enabled_() const {
  return this->status_ != nullptr || this->paper_out_sensor_ != nullptr || this->cover_open_sensor_ != nullptr ||
         this->error_sensor_ != nullptr;
}

void QR701::dump_config() {
  ESP_LOGCONFIG(TAG, "QR701 thermal receipt printer");
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::qr701
