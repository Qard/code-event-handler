#include <queue>              // cppcheck-suppress missingIncludeSystem
#include <unordered_map>      // cppcheck-suppress missingIncludeSystem

#include <node.h>             // cppcheck-suppress missingIncludeSystem
#include <node_object_wrap.h> // cppcheck-suppress missingIncludeSystem
#include <uv.h>               // cppcheck-suppress missingIncludeSystem
#include <v8-profiler.h>      // cppcheck-suppress missingIncludeSystem
#include <v8.h>               // cppcheck-suppress missingIncludeSystem

using node::ObjectWrap;
using v8::CodeEvent;
using v8::CodeEventHandler;
using v8::CodeEventType;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::JitCodeEvent;
using v8::Local;
using v8::NewStringType;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Value;

#if NODE_MAJOR_VERSION > 14 || (NODE_MAJOR_VERSION == 14 && NODE_MINOR_VERSION >= 5)
template<int N>
Local<String> intern(Isolate* isolate, const char(&literal)[N]) {
  return String::NewFromUtf8Literal(isolate, literal, NewStringType::kInternalized);
}
#else
Local<String> intern(Isolate* isolate, std::string message) {
  return String::NewFromUtf8(isolate, message.c_str(),
                             NewStringType::kInternalized, message.length())
      .ToLocalChecked();
}
#endif

Local<String> string(Isolate* isolate, std::string message) {
  return String::NewFromUtf8(isolate, message.c_str(),
                             NewStringType::kNormal, message.length())
      .ToLocalChecked();
}

class PerIsolateData {
 private:
  Global<FunctionTemplate> code_event_wrap_tmpl;
  Global<FunctionTemplate> code_event_handler_wrap_tmpl;

  PerIsolateData() {}

 public:
  static PerIsolateData* For(v8::Isolate* isolate);

  Global<FunctionTemplate>& CodeEventConstructor() {
    return code_event_wrap_tmpl;
  }
  Global<FunctionTemplate>& CodeEventHandlerConstructor() {
    return code_event_handler_wrap_tmpl;
  }
};

static std::unordered_map<v8::Isolate*, PerIsolateData> per_isolate_data_;

PerIsolateData* PerIsolateData::For(v8::Isolate* isolate) {
  auto maybe = per_isolate_data_.find(isolate);
  if (maybe != per_isolate_data_.end()) {
    return &maybe->second;
  }

  per_isolate_data_.emplace(std::make_pair(isolate, PerIsolateData()));

  auto pair = per_isolate_data_.find(isolate);
  auto perIsolateData = &pair->second;

  node::AddEnvironmentCleanupHook(isolate, [](void* data) {
    per_isolate_data_.erase(static_cast<v8::Isolate*>(data));
  }, isolate);

  return perIsolateData;
}

class CodeEventWrap : public ObjectWrap {
 private:
  uintptr_t address;
  uintptr_t previousAddress;
  size_t size;
  Global<String> functionName;
  Global<String> scriptName;
  int line;
  int column;
  CodeEventType type;
  std::string comment;

 public:
  explicit CodeEventWrap(Isolate* isolate, CodeEvent* code_event)
    : address(code_event->GetCodeStartAddress()),
      previousAddress(
// CodeEvent::GetPreviousCodeStartAddress didn't exist until Node.js 13.
#if NODE_MODULE_VERSION > 79
        code_event->GetPreviousCodeStartAddress()
#else
        0
#endif
      ),
      size(code_event->GetCodeSize()),
      functionName(isolate, code_event->GetFunctionName()),
      scriptName(isolate, code_event->GetScriptName()),
      line(code_event->GetScriptLine()),
      column(code_event->GetScriptColumn()),
      type(code_event->GetCodeType()),
      comment(code_event->GetComment()) {}

  static void GetAddress(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(Number::New(info.GetIsolate(), wrap->address));
  }

  static void GetPreviousAddress(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(Number::New(info.GetIsolate(), wrap->previousAddress));
  }

  static void GetSize(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(Number::New(info.GetIsolate(), wrap->size));
  }

  static void GetFunctionName(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(wrap->functionName.Get(info.GetIsolate()));
  }

  static void GetScriptName(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(wrap->scriptName.Get(info.GetIsolate()));
  }

  static void GetLine(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(Number::New(info.GetIsolate(), wrap->line));
  }

  static void GetColumn(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(Number::New(info.GetIsolate(), wrap->column));
  }

  static void GetType(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    std::string type_name = CodeEvent::GetCodeEventTypeName(wrap->type);
    info.GetReturnValue().Set(string(info.GetIsolate(), type_name));
  }

  static void GetComment(Local<String> name, const PropertyCallbackInfo<Value>& info) {
    CodeEventWrap* wrap = ObjectWrap::Unwrap<CodeEventWrap>(info.Holder());
    info.GetReturnValue().Set(string(info.GetIsolate(), wrap->comment));
  }

  static Local<FunctionTemplate> GetConstructorTemplate(Isolate* isolate) {
    auto per_isolate = PerIsolateData::For(isolate);
    if (!per_isolate->CodeEventConstructor().IsEmpty()) {
      return per_isolate->CodeEventConstructor().Get(isolate);
    }

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate);
    tpl->SetClassName(String::NewFromUtf8(isolate, "CodeEvent", NewStringType::kInternalized).ToLocalChecked());

    Local<ObjectTemplate> inst_tpl = tpl->InstanceTemplate();
    inst_tpl->SetInternalFieldCount(1);

    inst_tpl->SetAccessor(intern(isolate, "column"), GetColumn);
    inst_tpl->SetAccessor(intern(isolate, "line"), GetLine);
    inst_tpl->SetAccessor(intern(isolate, "scriptName"), GetScriptName);
    inst_tpl->SetAccessor(intern(isolate, "functionName"), GetFunctionName);
    inst_tpl->SetAccessor(intern(isolate, "comment"), GetComment);
    inst_tpl->SetAccessor(intern(isolate, "type"), GetType);
    inst_tpl->SetAccessor(intern(isolate, "size"), GetSize);
    inst_tpl->SetAccessor(intern(isolate, "previousAddress"), GetPreviousAddress);
    inst_tpl->SetAccessor(intern(isolate, "address"), GetAddress);

    per_isolate->CodeEventConstructor().Reset(isolate, tpl);

    return tpl;
  }

  static Local<Object> NewInstance(Isolate* isolate, CodeEvent* code_event) {
    auto inst = GetConstructorTemplate(isolate)->InstanceTemplate()
      ->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    auto obj = new CodeEventWrap(isolate, code_event);
    obj->Wrap(inst);

    return inst;
  }
};

class CodeEventHandlerWrap : public CodeEventHandler, public ObjectWrap {
 private:
  bool enabled = false;
  Global<Function> handler;
  std::queue<Global<Object>> events;
  Isolate* isolate_;
  uv_async_t async;

 public:
  explicit CodeEventHandlerWrap(Local<Function> handler)
    : CodeEventHandler(handler->GetIsolate()),
      handler(handler->GetIsolate(), handler),
      isolate_(handler->GetIsolate()) {}

  ~CodeEventHandlerWrap() {
    Disable();
  }

  static void OnEvent(uv_async_t* handle) {
    CodeEventHandlerWrap* code_events = static_cast<CodeEventHandlerWrap*>(handle->data);
    Isolate* isolate = code_events->isolate_;

    HandleScope scope(isolate);

    Local<Context> context = isolate->GetCurrentContext();
    Local<Function> fn = code_events->handler.Get(isolate);
    std::queue<Global<Object>>& queue = code_events->events;

    while (queue.size()) {
      HandleScope item_scope(isolate);
      auto event = queue.front().Get(isolate);
      queue.pop();

      const unsigned argc = 1;
      Local<Value> argv[argc] = {event};

      fn->Call(context, Null(isolate), argc, argv).ToLocalChecked();
    }
  }

  // cppcheck-suppress unusedFunction
  void Handle(CodeEvent* code_event) override {
    HandleScope scope(isolate_);
    events.emplace(isolate_, CodeEventWrap::NewInstance(isolate_, code_event));
    uv_async_send(&async);
  }

  static void New(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();

    if (!args[0]->IsFunction()) {
      auto message = intern(isolate, "Must provide a handler function");
      isolate->ThrowException(Exception::TypeError(message));
      return;
    }

    if (args.IsConstructCall()) {
      CodeEventHandlerWrap* obj = new CodeEventHandlerWrap(args[0].As<Function>());
      obj->Wrap(args.This());
      args.GetReturnValue().Set(args.This());
    } else {
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = args.Data().As<Object>()->GetInternalField(0).As<Function>();
      Local<Object> result = cons->NewInstance(context, argc, argv).ToLocalChecked();
      args.GetReturnValue().Set(result);
    }
  }

  void Enable() {
    if (enabled) return;

    enabled = true;
    uv_async_init(node::GetCurrentEventLoop(isolate_), &async, OnEvent);
    async.data = static_cast<void*>(this);
    CodeEventHandler::Enable();
  }

  static void EnableJS(const FunctionCallbackInfo<Value>& args) {
    CodeEventHandlerWrap* wrap = ObjectWrap::Unwrap<CodeEventHandlerWrap>(args.Holder());
    wrap->Enable();
  }

  void Disable() {
    if (!enabled) return;

    enabled = false;
    uv_close((uv_handle_t*)&async, [](uv_handle_t* handle) {});
    CodeEventHandler::Disable();
  }

  static void DisableJS(const FunctionCallbackInfo<Value>& args) {
    CodeEventHandlerWrap* wrap = ObjectWrap::Unwrap<CodeEventHandlerWrap>(args.Holder());
    wrap->Disable();
  }

  static Local<FunctionTemplate> GetConstructorTemplate(Isolate* isolate) {
    auto per_isolate = PerIsolateData::For(isolate);
    if (!per_isolate->CodeEventHandlerConstructor().IsEmpty()) {
      return per_isolate->CodeEventHandlerConstructor().Get(isolate);
    }

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(intern(isolate, "CodeEventHandler"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "enable", EnableJS);
    NODE_SET_PROTOTYPE_METHOD(tpl, "disable", DisableJS);

    per_isolate->CodeEventHandlerConstructor().Reset(isolate, tpl);

    return tpl;
  }

  static Local<Function> GetConstructor(Isolate* isolate) {
    return GetConstructorTemplate(isolate)
      ->GetFunction(isolate->GetCurrentContext())
      .ToLocalChecked();
  }
};

NODE_MODULE_INIT(/* exports, module, context */) {
  Isolate* isolate = context->GetIsolate();

  module.As<Object>()->Set(context, intern(isolate, "exports"),
    CodeEventHandlerWrap::GetConstructor(isolate)).FromJust();
}
