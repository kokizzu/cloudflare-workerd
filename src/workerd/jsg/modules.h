// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#pragma once

#include <workerd/jsg/function.h>
#include <workerd/jsg/modules.capnp.h>
#include <workerd/jsg/observer.h>
#include <workerd/jsg/promise.h>
#include <workerd/util/sentry.h>
#include <workerd/util/thread-scopes.h>

#include <v8-json.h>

#include <kj/filesystem.h>
#include <kj/map.h>

namespace workerd::jsg {

enum class InstantiateModuleOptions {
  // Allows pending top-level await in the module when evaluated. Will cause
  // the microtask queue to be drained once in an attempt to resolve those.
  DEFAULT,
  // Throws if the module evaluation results in a pending promise.
  NO_TOP_LEVEL_AWAIT,
};

void instantiateModule(jsg::Lock& js,
    v8::Local<v8::Module>& module,
    InstantiateModuleOptions options = InstantiateModuleOptions::DEFAULT);

enum class ModuleInfoCompileOption {
  // The BUNDLE options tells the compile operation to treat the content as coming
  // from a worker bundle.
  BUNDLE,

  // The BUILTIN option tells the compile operation to treat the content as a builtin
  // module. This implies certain changes in behavior, such as treating the content
  // as an immutable, process-lifetime buffer that will never be destroyed, and caching
  // the compilation data.
  BUILTIN,
};

v8::Local<v8::WasmModuleObject> compileWasmModule(
    jsg::Lock& js, kj::ArrayPtr<const uint8_t> code, const CompilationObserver& observer);

// The ModuleRegistry maintains the collection of modules known to a script that can be
// required or imported.
class ModuleRegistry {
 public:
  KJ_DISALLOW_COPY_AND_MOVE(ModuleRegistry);

  ModuleRegistry() {}

  using Type = ModuleType;

  JSG_MEMORY_INFO(ModuleRegistry) {
    // TODO(soon): Implement memory tracking for ModuleRegistry
  }

  enum class ResolveOption {
    // Default resolution. Check the worker bundle first, then builtins.
    DEFAULT,
    // Built-in resolution. Check only non-internal builtins.
    BUILTIN_ONLY,
    // Internal resolution. Check only internal builtins.
    INTERNAL_ONLY,
  };

  static inline ModuleRegistry* from(jsg::Lock& js) {
    return static_cast<ModuleRegistry*>(js.v8Context()->GetAlignedPointerFromEmbedderData(2));
  }

  struct CapnpModuleInfo {
    Value fileScope;                                       // default import
    kj::HashMap<kj::StringPtr, jsg::Value> topLevelDecls;  // named imports

    CapnpModuleInfo(Value fileScope, kj::HashMap<kj::StringPtr, jsg::Value> topLevelDecls);
    CapnpModuleInfo(CapnpModuleInfo&&) = default;
    CapnpModuleInfo& operator=(CapnpModuleInfo&&) = default;
  };

  struct CommonJsModuleInfo {
    struct CommonJsModuleProvider {
      virtual JsObject getContext(Lock& js) = 0;
      virtual JsValue getExports(Lock& js) = 0;
      virtual ~CommonJsModuleProvider() noexcept(false) = default;
    };

    kj::Own<CommonJsModuleProvider> provider;
    jsg::Function<void()> evalFunc;

    CommonJsModuleInfo(auto& lock,
        kj::StringPtr name,
        kj::StringPtr content,
        kj::Own<CommonJsModuleProvider> provider)
        : provider(kj::mv(provider)),
          evalFunc(initEvalFunc(lock, *this->provider, name, content)) {}

    CommonJsModuleInfo(CommonJsModuleInfo&&) = default;
    CommonJsModuleInfo& operator=(CommonJsModuleInfo&&) = default;

    jsg::JsValue getExports(jsg::Lock& js);

    static jsg::Function<void()> initEvalFunc(
        auto& lock, CommonJsModuleProvider& provider, kj::StringPtr name, kj::StringPtr content) {
      v8::ScriptOrigin origin(v8StrIntern(lock.v8Isolate, name));
      v8::ScriptCompiler::Source source(v8Str(lock.v8Isolate, content), origin);
      auto context = lock.v8Context();
      v8::Local<v8::Object> handle = provider.getContext(lock);
      auto fn =
          jsg::check(v8::ScriptCompiler::CompileFunction(context, &source, 0, nullptr, 1, &handle));
      return lock.template unwrap<jsg::Function<void()>>(context, fn);
    }
  };

  template <typename T>
  struct ValueModuleInfo {
    jsg::V8Ref<T> value;

    ValueModuleInfo(jsg::Lock& js, v8::Local<T> value): value(js.v8Isolate, value) {}

    ValueModuleInfo(ValueModuleInfo&&) = default;
    ValueModuleInfo& operator=(ValueModuleInfo&&) = default;
  };

  using DataModuleInfo = ValueModuleInfo<v8::ArrayBuffer>;
  using TextModuleInfo = ValueModuleInfo<v8::String>;
  using WasmModuleInfo = ValueModuleInfo<v8::WasmModuleObject>;
  using JsonModuleInfo = ValueModuleInfo<v8::Value>;
  using ObjectModuleInfo = ValueModuleInfo<v8::Object>;

  struct ModuleInfo {
    HashableV8Ref<v8::Module> module;

    using SyntheticModuleInfo = kj::OneOf<CapnpModuleInfo,
        CommonJsModuleInfo,
        DataModuleInfo,
        TextModuleInfo,
        WasmModuleInfo,
        JsonModuleInfo,
        ObjectModuleInfo>;
    kj::Maybe<SyntheticModuleInfo> maybeSynthetic;
    kj::Maybe<kj::Array<kj::String>> maybeNamedExports;

    ModuleInfo(jsg::Lock& js,
        v8::Local<v8::Module> module,
        kj::Maybe<SyntheticModuleInfo> maybeSynthetic = kj::none);

    ModuleInfo(jsg::Lock& js,
        kj::StringPtr name,
        kj::ArrayPtr<const char> content,
        kj::ArrayPtr<const kj::byte> compileCache,
        ModuleInfoCompileOption flags,
        const CompilationObserver& observer);

    ModuleInfo(jsg::Lock& js,
        kj::StringPtr name,
        kj::Maybe<kj::ArrayPtr<const kj::StringPtr>> maybeExports,
        SyntheticModuleInfo synthetic);

    ModuleInfo(ModuleInfo&&) = default;
    ModuleInfo& operator=(ModuleInfo&&) = default;

    uint hashCode() const {
      return module.hashCode();
    }
  };

  struct ModuleRef {
    const kj::Path& specifier;
    Type type;
    ModuleInfo& module;
  };

  enum class ResolveMethod {
    // Resolving using the standard static or dynamic import.
    IMPORT,
    // Resolving using the commonjs require method.
    REQUIRE,
  };

  using ModuleCallback =
      kj::Function<kj::Maybe<ModuleInfo>(Lock&, ResolveMethod, kj::Maybe<const kj::Path&>&)>;

  virtual kj::Maybe<ModuleInfo&> resolve(jsg::Lock& js,
      const kj::Path& specifier,
      kj::Maybe<const kj::Path&> referrer = kj::none,
      ResolveOption option = ResolveOption::DEFAULT,
      ResolveMethod method = ResolveMethod::IMPORT,
      kj::Maybe<kj::StringPtr> rawSpecifier = kj::none) = 0;

  virtual kj::Maybe<ModuleRef> resolve(jsg::Lock& js, v8::Local<v8::Module> module) = 0;

  virtual Promise<Value> resolveDynamicImport(jsg::Lock& js,
      const kj::Path& specifier,
      const kj::Path& referrer,
      kj::StringPtr rawSpecifier) = 0;

  virtual Value resolveInternalImport(jsg::Lock& js, const kj::StringPtr specifier) = 0;

  // The dynamic import callback is provided by the embedder to set up any context necessary
  // for instantiating the module during a dynamic import. The handler function passed into
  // the callback is called to actually perform the instantiation of the module.
  using DynamicImportCallback = Promise<Value>(jsg::Lock& js, kj::Function<Value()> handler);

  virtual void setDynamicImportCallback(kj::Function<DynamicImportCallback> func) = 0;

  enum class RequireImplOptions {
    // Require returns the module namespace.
    DEFAULT,
    // Require returns the default export.
    EXPORT_DEFAULT,
  };

  static JsValue requireImpl(
      Lock& js, ModuleInfo& info, RequireImplOptions options = RequireImplOptions::DEFAULT);
};

template <typename TypeWrapper>
v8::MaybeLocal<v8::Promise> dynamicImportCallback(v8::Local<v8::Context> context,
    v8::Local<v8::Data> host_defined_options,
    v8::Local<v8::Value> resource_name,
    v8::Local<v8::String> specifier,
    v8::Local<v8::FixedArray> import_attributes);

kj::Maybe<kj::OneOf<kj::String, ModuleRegistry::ModuleInfo>> tryResolveFromFallbackService(Lock& js,
    const kj::Path& specifier,
    kj::Maybe<const kj::Path&>& referrer,
    CompilationObserver& observer,
    ModuleRegistry::ResolveMethod method,
    kj::Maybe<kj::StringPtr> rawSpecifier);

template <typename TypeWrapper>
class ModuleRegistryImpl final: public ModuleRegistry {
 public:
  KJ_DISALLOW_COPY_AND_MOVE(ModuleRegistryImpl);

  ModuleRegistryImpl(CompilationObserver& observer): observer(observer) {}

  static kj::Own<ModuleRegistryImpl<TypeWrapper>> install(
      v8::Isolate* isolate, v8::Local<v8::Context> context, CompilationObserver& observer) {
    auto registry = kj::heap<ModuleRegistryImpl<TypeWrapper>>(observer);
    context->SetAlignedPointerInEmbedderData(2, registry.get());
    isolate->SetHostImportModuleDynamicallyCallback(dynamicImportCallback<TypeWrapper>);
    return kj::mv(registry);
  }

  static inline ModuleRegistryImpl* from(jsg::Lock& js) {
    return static_cast<ModuleRegistryImpl*>(js.v8Context()->GetAlignedPointerFromEmbedderData(2));
  }

  void setDynamicImportCallback(kj::Function<DynamicImportCallback> func) override {
    dynamicImportHandler = kj::mv(func);
  }

  void add(kj::Path& specifier, ModuleInfo&& info) {
    entries.insert(kj::heap<Entry>(specifier, Type::BUNDLE, kj::fwd<ModuleInfo>(info)));
  }

  void addBuiltinModule(Module::Reader module) {
    if (module.which() != Module::SRC) {
      auto specifier = module.getName();
      auto path = kj::Path::parse(specifier);
      switch (module.which()) {
        case Module::WASM:
          // The body of this callback is copied from `compileWasmGlobal` in
          // src/workerd/server/workerd-api.c++.
          addBuiltinModule(specifier,
              [specifier, module, this](Lock& lock, ResolveMethod, kj::Maybe<const kj::Path&>&) {
            lock.setAllowEval(true);
            KJ_DEFER(lock.setAllowEval(false));

            // Allow Wasm compilation to spawn a background thread for tier-up, i.e.
            // recompiling Wasm with optimizations in the background. Otherwise Wasm startup
            // is way too slow. Until tier-up finishes, requests will be handled using
            // Liftoff-generated code, which compiles fast but runs slower.
            AllowV8BackgroundThreadsScope scope;
            auto wasmModule =
                jsg::compileWasmModule(lock, module.getWasm().asBytes(), this->observer);
            return jsg::ModuleRegistry::ModuleInfo(
                lock, specifier, kj::none, jsg::ModuleRegistry::WasmModuleInfo(lock, wasmModule));
          }, module.getType());
          return;
        case Module::DATA:
          addBuiltinModule(specifier,
              [specifier, module](Lock& lock, ResolveMethod, kj::Maybe<const kj::Path&>&) {
            v8::Local<v8::ArrayBuffer> data =
                lock.wrapBytes(kj::heapArray(module.getData().asBytes()));
            return jsg::ModuleRegistry::ModuleInfo(
                lock, specifier, kj::none, jsg::ModuleRegistry::DataModuleInfo(lock, data));
          }, module.getType());
          return;
        case Module::JSON:
          addBuiltinModule(specifier,
              [specifier, module](Lock& lock, ResolveMethod, kj::Maybe<const kj::Path&>&) {
            auto data =
                jsg::check(v8::JSON::Parse(lock.v8Context(), lock.wrapString(module.getJson())));
            return jsg::ModuleRegistry::ModuleInfo(
                lock, specifier, kj::none, jsg::ModuleRegistry::JsonModuleInfo(lock, data));
          }, module.getType());
          return;
        case Module::SRC:
          KJ_UNREACHABLE
      }
    }
    // TODO: asChars() might be wrong for wide characters
    addBuiltinModule(module.getName(), module.getSrc().asChars(), module.getType(),
        module.getCompileCache().asBytes());
  }

  void addBuiltinBundle(Bundle::Reader bundle, kj::Maybe<Type> maybeFilter = kj::none) {
    for (auto module: bundle.getModules()) {
      if (module.getType() == maybeFilter.orDefault(module.getType())) addBuiltinModule(module);
    }
  }

  template <typename Func>
  void addBuiltinBundleFiltered(Bundle::Reader bundle, Func filter) {
    for (auto module: bundle.getModules()) {
      if (filter(module)) {
        addBuiltinModule(module);
      }
    }
  }

  // Register new module accessible by a given importPath. The module is instantiated
  // after first resolve attempt within application has failed, i.e. it is possible for
  // application to override the module.
  // sourceCode has to exist while this ModuleRegistry exists.
  // The expectation is for this method to be called during the assembly of worker global context
  // after registering all user modules.
  void addBuiltinModule(kj::StringPtr specifier,
      kj::ArrayPtr<const char> sourceCode,
      Type type = Type::BUILTIN,
      kj::ArrayPtr<const kj::byte> compileCache = {}) {
    KJ_ASSERT(type != Type::BUNDLE);
    auto path = kj::Path::parse(specifier);
    entries.insert(kj::heap<Entry>(path, type, sourceCode, compileCache));
  }

  void addBuiltinModule(
      kj::StringPtr specifier, ModuleCallback factory, Type type = Type::BUILTIN) {
    KJ_ASSERT(type != Type::BUNDLE);
    auto path = kj::Path::parse(specifier);
    entries.insert(kj::heap<Entry>(path, type, kj::mv(factory)));
  }

  template <typename T>
  void addBuiltinModule(kj::StringPtr specifier, Type type = Type::BUILTIN) {
    addBuiltinModule(specifier, alloc<T>(), type);
  }

  template <typename T>
  void addBuiltinModule(kj::StringPtr specifier, Ref<T> object, Type type = Type::BUILTIN) {
    addBuiltinModule(specifier,
        [specifier = kj::str(specifier), object = kj::mv(object)](
            Lock& js, ResolveMethod, kj::Maybe<const kj::Path&>&) mutable -> kj::Maybe<ModuleInfo> {
      auto& wrapper = TypeWrapper::from(js.v8Isolate);
      auto wrap = wrapper.wrap(js, js.v8Context(), kj::none, kj::mv(object));
      return kj::Maybe(ModuleInfo(js, specifier, kj::none, ObjectModuleInfo(js, wrap)));
    },
        type);
  }

  kj::Maybe<ModuleInfo&> resolve(jsg::Lock& js,
      const kj::Path& specifier,
      kj::Maybe<const kj::Path&> referrer = kj::none,
      ResolveOption option = ResolveOption::DEFAULT,
      ResolveMethod method = ResolveMethod::IMPORT,
      kj::Maybe<kj::StringPtr> rawSpecifier = kj::none) override {
    using Key = typename Entry::Key;
    if (option == ResolveOption::INTERNAL_ONLY) {
      KJ_IF_SOME(entry, entries.find(Key(specifier, Type::INTERNAL))) {
        return entry->module(js, observer, referrer, method);
      }
      return kj::none;
    } else if (option == ResolveOption::BUILTIN_ONLY) {
      KJ_IF_SOME(entry, entries.find(Key(specifier, Type::BUILTIN))) {
        return entry->module(js, observer, referrer, method);
      }
    } else {
      if (option == ResolveOption::DEFAULT) {
        // First, we try to resolve a worker bundle version of the module.
        KJ_IF_SOME(entry, entries.find(Key(specifier, Type::BUNDLE))) {
          return entry->module(js, observer, referrer, method);
        }
      }
      // Then we look for a built-in version of the module.
      KJ_IF_SOME(entry, entries.find(Key(specifier, Type::BUILTIN))) {
        return entry->module(js, observer, referrer, method);
      }
    }

    // An internal only resolution should never go to the fallback service
    KJ_DASSERT(option != ResolveOption::INTERNAL_ONLY);

    // If the module is not found and we have a module fallback service configured,
    // let's try that as a means of looking it up.
    auto str = specifier.toString(true);
    KJ_IF_SOME(found, fallbackServiceRedirects.find(str)) {
      // The fallback service has already given us a redirect response for this specifier.
      // let's use it to try to resolve. Make sure we're using DEFAULT resolution so BUNDLE-typed
      // modules from the fallback service can be used.
      option = ResolveOption::DEFAULT;
      return resolve(js, specifier.parent().eval(found), referrer, option, method, rawSpecifier);
    }
    KJ_IF_SOME(info,
        tryResolveFromFallbackService(js, specifier, referrer, observer, method, rawSpecifier)) {
      // If we resolved a module from the fallback service, we have to be sure
      // to add it to the registry...
      KJ_SWITCH_ONEOF(info) {
        KJ_CASE_ONEOF(i, ModuleInfo) {
          auto type = Type::BUNDLE;
          if (option == ResolveOption::BUILTIN_ONLY) {
            if (str.startsWith("/node:") || str.startsWith("/cloudflare:") ||
                str.startsWith("/workerd:")) {
              type = Type::BUILTIN;
            }
          }

          entries.insert(kj::heap<Entry>(specifier, type, kj::mv(i)));
          auto& entry = KJ_ASSERT_NONNULL(entries.find(Key(specifier, type)));
          return entry->module(js, observer, referrer, method);
        }
        KJ_CASE_ONEOF(s, kj::String) {
          // If a kj::String is returned, it means the fallback service is redirecting
          // us to another module that should already be in the registry... or could
          // itself end up calling back to the fallback service.
          fallbackServiceRedirects.upsert(kj::mv(str), kj::str(s));
          // Make sure we're using DEFAULT resolution so BUNDLE-typed modules from the fallback
          // service can be used.
          option = ResolveOption::DEFAULT;
          return resolve(js, specifier.parent().eval(s), referrer, option, method, rawSpecifier);
        }
      }
    }

    return kj::none;
  }

  kj::Maybe<ModuleRef> resolve(jsg::Lock& js, v8::Local<v8::Module> module) override {
    for (const kj::Own<Entry>& entry: entries) {
      // Unfortunately we cannot use entries.find(...) in here because the module info can
      // be initialized lazily at any point after the entry is indexed, making the lookup
      // by module a bit problematic. Iterating through the entries is slower but it works.
      KJ_IF_SOME(info, entry->info.template tryGet<ModuleInfo>()) {
        if (info.module == module) {
          return ModuleRef{
            .specifier = entry->specifier,
            .type = entry->type,
            .module = const_cast<ModuleInfo&>(info),
          };
        }
      }
    }
    return kj::none;
  }

  size_t size() const {
    return entries.size();
  }

  Promise<Value> resolveDynamicImport(jsg::Lock& js,
      const kj::Path& specifier,
      const kj::Path& referrer,
      kj::StringPtr rawSpecifier) override {
    // Here, we first need to determine if the referrer is a built-in module
    // or not. If it is a built-in, then we are only permitted to resolve
    // internal modules. If the worker bundle provided an override for the
    // built-in module, then the built-in was never registered and won't
    // be found.
    using Key = typename Entry::Key;
    auto resolveOption = ModuleRegistry::ResolveOption::DEFAULT;
    if (entries.find(Key(referrer, Type::BUILTIN)) != kj::none) {
      resolveOption = ModuleRegistry::ResolveOption::INTERNAL_ONLY;
    }

    KJ_IF_SOME(info,
        resolve(js, specifier, referrer, resolveOption, ResolveMethod::IMPORT, rawSpecifier)) {
      KJ_IF_SOME(func, dynamicImportHandler) {
        auto handler = [&info, isolate = js.v8Isolate]() -> Value {
          auto& js = Lock::from(isolate);
          auto module = info.module.getHandle(js);
          instantiateModule(js, module);
          return js.v8Ref(module->GetModuleNamespace());
        };
        return func(js, kj::mv(handler));
      }

      // If there is no dynamicImportHandler set, then we are going to handle that as if
      // the module does not exist and fall through to the rejected promise below.
    }

    return js.rejectedPromise<Value>(
        js.v8Error(kj::str("No such module \"", specifier.toString(), "\".")));
  }

  Value resolveInternalImport(jsg::Lock& js, const kj::StringPtr specifier) override {
    auto specifierPath = kj::Path(specifier);
    auto resolveOption = jsg::ModuleRegistry::ResolveOption::INTERNAL_ONLY;
    auto maybeModuleInfo =
        resolve(js, specifierPath, kj::none, resolveOption, ResolveMethod::IMPORT, specifier);
    auto moduleInfo = &KJ_REQUIRE_NONNULL(maybeModuleInfo, "No such module \"", specifier, "\".");
    auto handle = moduleInfo->module.getHandle(js);
    jsg::instantiateModule(js, handle);
    return js.v8Ref(handle->GetModuleNamespace());
  }

  CompilationObserver& getObserver() {
    return observer;
  }

 private:
  CompilationObserver& observer;
  kj::Maybe<kj::Function<DynamicImportCallback>> dynamicImportHandler;

  // When we build a bundle containing modules, we must build a table of modules to resolve imports.
  //
  // Because of the design of V8's resolver callback, we end up needing a table with two indexes:
  // we need to be able to search it by path (filename) as well as search for a specific module
  // object by identity. We use a kj::Table!
  struct Entry {
    using Info = kj::OneOf<ModuleInfo, kj::ArrayPtr<const char>, ModuleCallback>;

    struct Key {
      const kj::Path& specifier;
      const Type type = Type::BUNDLE;
      uint hash;

      Key(const kj::Path& specifier, Type type)
          : specifier(specifier),
            type(type),
            hash(kj::hashCode(specifier, type)) {}

      uint hashCode() const {
        return hash;
      }
    };

    kj::Path specifier;
    Type type;

    // Either instantiated module or module source code.
    Info info;

    // Optional compileCache.
    kj::ArrayPtr<const kj::byte> compileCache;

    Entry(const kj::Path& specifier, Type type, ModuleInfo info)
        : specifier(specifier.clone()),
          type(type),
          info(kj::mv(info)) {}

    Entry(const kj::Path& specifier,
        Type type,
        kj::ArrayPtr<const char> src,
        kj::ArrayPtr<const kj::byte> compileCache)
        : specifier(specifier.clone()),
          type(type),
          info(src),
          compileCache(compileCache) {}

    Entry(const kj::Path& specifier, Type type, ModuleCallback factory)
        : specifier(specifier.clone()),
          type(type),
          info(kj::mv(factory)) {}

    Entry(Entry&&) = default;
    Entry& operator=(Entry&&) = default;

    // Lazily instantiate module from source code if needed
    kj::Maybe<ModuleInfo&> module(jsg::Lock& js,
        CompilationObserver& observer,
        kj::Maybe<const kj::Path&> referrer,
        ModuleRegistry::ResolveMethod method = ModuleRegistry::ResolveMethod::IMPORT) {
      KJ_SWITCH_ONEOF(info) {
        KJ_CASE_ONEOF(moduleInfo, ModuleInfo) {
          return kj::Maybe<ModuleInfo&>(moduleInfo);
        }
        KJ_CASE_ONEOF(src, kj::ArrayPtr<const char>) {
          info = ModuleInfo(js, specifier.toString(), src, compileCache,
              ModuleInfoCompileOption::BUILTIN, observer);
          return info.tryGet<ModuleInfo>();
        }
        KJ_CASE_ONEOF(src, ModuleCallback) {
          KJ_IF_SOME(result, src(js, method, referrer)) {
            info = kj::mv(result);
          }
          return info.tryGet<ModuleInfo>();
        }
      }
      KJ_UNREACHABLE;
    }
  };

  struct SpecifierHashCallbacks {
    using Key = typename Entry::Key;

    const Key keyForRow(const kj::Own<Entry>& row) const {
      return Key(row->specifier, row->type);
    }

    bool matches(const kj::Own<Entry>& row, Key key) const {
      return row->specifier == key.specifier && row->type == key.type;
    }

    uint hashCode(Key key) const {
      return key.hashCode();
    }
  };

  kj::Table<kj::Own<Entry>, kj::HashIndex<SpecifierHashCallbacks>> entries;
  kj::HashMap<kj::String, kj::String> fallbackServiceRedirects;
};

template <typename TypeWrapper>
v8::MaybeLocal<v8::Promise> dynamicImportCallback(v8::Local<v8::Context> context,
    v8::Local<v8::Data> host_defined_options,
    v8::Local<v8::Value> resource_name,
    v8::Local<v8::String> specifier,
    v8::Local<v8::FixedArray> import_attributes) {
  auto& js = Lock::current();
  auto registry = ModuleRegistry::from(js);
  auto& wrapper = TypeWrapper::from(js.v8Isolate);

  // TODO(cleanup): This could probably be simplified using jsg::Promise
  const auto makeRejected = [&](auto reason) {
    v8::Local<v8::Promise::Resolver> resolver;
    if (v8::Promise::Resolver::New(context).ToLocal(&resolver) &&
        resolver->Reject(context, reason).IsJust()) {
      return resolver->GetPromise();
    }
    return v8::Local<v8::Promise>();
  };

  // The specification for import attributes strongly recommends that embedders
  // reject import attributes and types they do not understand/implement. This
  // is because import attributes can alter the interpretation of a module and
  // are considered to be part of the unique key for caching a module.
  // Throwing an error for things we do not understand is the safest thing to do.
  // However, historically we have not followed this guideline in the spec
  // and unfortunately there are applications deployed that will break if we
  // started enforcing that guideline without a compat flag.
  if (!import_attributes.IsEmpty() && import_attributes->Length() > 0 &&
      js.getThrowOnUnrecognizedImportAssertion()) {
    return makeRejected(js.v8Error("Unrecognized import attributes specified"));
  }

  // The dynamic import might be resolved synchronously or asynchronously.
  // Accordingly, resolveDynamicImport will return a jsg::Promise<jsg::Value>
  // that will resolve to the module's namespace object or will reject if there
  // was any error.
  //
  // Importantly, we defensively catch any synchronous errors here and handle them
  // explicitly as rejected Promises.
  v8::TryCatch tryCatch(js.v8Isolate);

  // TODO(cleanup): If kj::Path::parse or kj::Path::eval fail it is most likely the application's
  // fault. We'll return a "No such module" error. We could handle this more gracefully
  // if kj::Path had tryParse()/tryEval() variants.

  auto maybeReferrerPath = ([&]() -> kj::Maybe<kj::Path> {
    try {
      return kj::Path::parse(kj::str(resource_name));
    } catch (kj::Exception& ex) {
      return kj::none;
    }
  })();

  auto spec = kj::str(specifier);
  if (isNodeJsCompatEnabled(js)) {
    KJ_IF_SOME(nodeSpec, checkNodeSpecifier(spec)) {
      spec = kj::mv(nodeSpec);
    }
  }

  // Handle process module redirection based on enable_nodejs_process_v2 flag
  if (spec == "node:process") {
    auto processSpec = isNodeJsProcessV2Enabled(js) ? "node-internal:public_process"_kj
                                                    : "node-internal:legacy_process"_kj;
    try {
      // Use resolveInternalImport for internal modules
      auto moduleNamespace = registry->resolveInternalImport(js, processSpec);
      v8::Local<v8::Promise::Resolver> resolver;
      if (v8::Promise::Resolver::New(context).ToLocal(&resolver) &&
          resolver->Resolve(context, moduleNamespace.getHandle(js)).IsJust()) {
        return resolver->GetPromise();
      }
      return v8::Local<v8::Promise>();
    } catch (JsExceptionThrown&) {
      if (!tryCatch.CanContinue() || tryCatch.Exception().IsEmpty()) {
        return v8::MaybeLocal<v8::Promise>();
      }
      return makeRejected(tryCatch.Exception());
    } catch (kj::Exception& ex) {
      return makeRejected(makeInternalError(js.v8Isolate, kj::mv(ex)));
    }
  }

  auto maybeSpecifierPath = ([&]() -> kj::Maybe<kj::Path> {
    // If the specifier begins with one of our known prefixes, let's not resolve
    // it against the referrer.
    if (spec.startsWith("node:") || spec.startsWith("cloudflare:") || spec.startsWith("workerd:")) {
      return kj::Path::parse(spec);
    }
    KJ_IF_SOME(referrerPath, maybeReferrerPath) {
      try {
        return referrerPath.parent().eval(spec);
      } catch (kj::Exception& ex) {
        return kj::none;
      }
    }
    return kj::none;
  })();

  if (maybeReferrerPath == kj::none || maybeSpecifierPath == kj::none) {
    // If either of these are nullptr it means the kj::Path::parse or
    // kj::Path::eval failed. We want to handle these as No such module
    // errors.
    return makeRejected(js.v8Error(kj::str("No such module \"", specifier, "\"")));
  }

  auto& referrerPath = KJ_ASSERT_NONNULL(maybeReferrerPath);
  auto& specifierPath = KJ_ASSERT_NONNULL(maybeSpecifierPath);

  try {
    return wrapper.wrap(js, context, kj::none,
        registry->resolveDynamicImport(js, specifierPath, referrerPath, spec));
  } catch (JsExceptionThrown&) {
    // If the tryCatch.Exception().IsEmpty() here is true, no JavaScript error
    // was scheduled which can happen in a few edge cases. Treat it as if
    // CanContinue() is false.
    if (!tryCatch.CanContinue() || tryCatch.Exception().IsEmpty()) {
      // There's nothing else we can reasonably do.
      return v8::MaybeLocal<v8::Promise>();
    }

    return makeRejected(tryCatch.Exception());
  } catch (kj::Exception& ex) {
    return makeRejected(makeInternalError(js.v8Isolate, kj::mv(ex)));
  }
  KJ_UNREACHABLE;
}

ModuleRegistry* getModulesForResolveCallback(v8::Isolate* isolate);

}  // namespace workerd::jsg
