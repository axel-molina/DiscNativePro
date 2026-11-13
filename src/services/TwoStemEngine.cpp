#include "TwoStemEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <onnxruntime_c_api.h>
#include <kiss_fft.h>
#include <kiss_fftr.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace
{
constexpr int kFft = 6144;      // n_fft del modelo MDX (3072 bins)
constexpr int kHop = 1024;
constexpr int kFrames = 256;    // frames por chunk del modelo
constexpr int kHalf = 128;      // avance entre chunks (solape 50 %)
constexpr int kBins = 3072;     // el bin 3073 (Nyquist) se descarta
constexpr int kSpecElems = 4 * kBins * kFrames; // [4, 3072, 256]
constexpr int64_t kAdvance = (int64_t) kHalf * kHop;              // 131072
constexpr int64_t kNeed = (int64_t)(kFrames - 1) * kHop + kFft;   // 267264

const OrtApi* ortApi()
{
    static const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    return api;
}

#define ORT_CHECK(call, errOut)                                                    \
    do                                                                             \
    {                                                                              \
        if (auto* st__ = (call))                                                   \
        {                                                                          \
            if (errOut != nullptr)                                                 \
                *errOut = juce::String::fromUTF8(ortApi()->GetErrorMessage(st__)); \
            ortApi()->ReleaseStatus(st__);                                         \
            return false;                                                          \
        }                                                                          \
    } while (false)

juce::File appSupportDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/DiscNativePro/models");
}
} // namespace

juce::File TwoStemEngine::modelFile()
{
    const auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const juce::String name = "uvr_mdxnet_voc_ft_fp16.onnx";

    // .app bundle: Contents/Resources/models/
    auto bundleModel = exe.getParentDirectory().getParentDirectory()
                           .getChildFile("Resources/models").getChildFile(name);
    if (bundleModel.existsAsFile())
        return bundleModel;

    // Windows/Linux: <exe dir>/models/
    auto exeModel = exe.getParentDirectory().getChildFile("models").getChildFile(name);
    if (exeModel.existsAsFile())
        return exeModel;

    auto appModel = appSupportDir().getChildFile(name);
    if (appModel.existsAsFile())
        return appModel;

    return {};
}

bool TwoStemEngine::isModelAvailable()
{
    return modelFile().existsAsFile();
}

struct TwoStemEngine::Impl
{
    OrtEnv* env = nullptr;
    OrtSession* session = nullptr;
    OrtSessionOptions* options = nullptr;
    OrtMemoryInfo* memInfo = nullptr;
    juce::String inputName = "input";
    juce::String outputName = "output";

    kiss_fftr_cfg fftCfg = nullptr;
    kiss_fftr_cfg ifftCfg = nullptr;

    // Ventanas: Hann simétrico 6144 para el análisis/síntesis temporal (igual
    // que el banco de referencia) y Hann periódico 256 solo para promediar los
    // chunks solapados en el dominio del espectro.
    std::vector<float> stftWindow;   // 6144
    std::vector<float> chunkWindow;  // 256 (np.hanning(257)[:256])
    std::vector<float> prevSpecW;    // [4, 128, 3072] salida previa ponderada
    bool hasPrev = false;
    bool flushDone = false;

    // Estado del stream (todas las posiciones en muestras absolutas 44.1k).
    int64_t absIn = 0;       // muestras de entrada alimentadas
    int64_t inBufStart = 0;  // muestra absoluta de inL/inR[0]
    std::vector<float> inL, inR;

    int64_t chunkStart = 0;  // posición del próximo chunk (avanza kAdvance)
    int64_t outStart = 0;    // próxima muestra a emitir
    int64_t accStart = 0;    // muestra absoluta de vocL[0] (acumulador)
    std::vector<float> vocL, vocR, wsum; // instrumental se calcula al emitir

    std::vector<float> chunkIn, chunkOut;
    std::vector<kiss_fft_cpx> frameSpec;
    std::vector<float> frameL, frameR, frameTimeL, frameTimeR;

    // ~Impl no libera ORT (mismo rationale que OnnxStemEngine: teardown
    // estático de exit() aborta dentro de Environment::~Environment).
    ~Impl() = default;

    // Ejecuta el modelo para el chunk de frames [start/1024 ... +255]. Requiere
    // absIn - start >= kNeed.
    bool runChunkAt(int64_t start, juce::String* error)
    {
        auto* api = ortApi();

        // 1. STFT por frame → chunkIn [4, 3072, 256] con el orden del banco:
        //    canales = L.re, R.re, L.im, R.im.
        std::fill(chunkIn.begin(), chunkIn.end(), 0.0f);
        for (int f = 0; f < kFrames; ++f)
        {
            const int64_t off = start + (int64_t) f * kHop - inBufStart;
            for (int i = 0; i < kFft; ++i)
            {
                const int64_t p = off + i;
                const float l = (p >= 0 && p < (int64_t) inL.size()) ? inL[(size_t) p] : 0.0f;
                const float r = (p >= 0 && p < (int64_t) inR.size()) ? inR[(size_t) p] : 0.0f;
                const float w = stftWindow[(size_t) i];
                frameL[(size_t) i] = l * w;
                frameR[(size_t) i] = r * w;
            }
            kiss_fftr(fftCfg, frameL.data(), frameSpec.data());
            for (int b = 0; b < kBins; ++b)
            {
                chunkIn[((size_t)(0 * kBins + b)) * kFrames + (size_t) f] = frameSpec[(size_t) b].r;
                chunkIn[((size_t)(2 * kBins + b)) * kFrames + (size_t) f] = frameSpec[(size_t) b].i;
            }
            kiss_fftr(fftCfg, frameR.data(), frameSpec.data());
            for (int b = 0; b < kBins; ++b)
            {
                chunkIn[((size_t)(1 * kBins + b)) * kFrames + (size_t) f] = frameSpec[(size_t) b].r;
                chunkIn[((size_t)(3 * kBins + b)) * kFrames + (size_t) f] = frameSpec[(size_t) b].i;
            }
        }

        // 2. Modelo
        const int64_t dims[4] = { 1, 4, kBins, kFrames };
        OrtValue* in = nullptr;
        if (api->CreateTensorWithDataAsOrtValue(memInfo, chunkIn.data(),
                                                chunkIn.size() * sizeof(float),
                                                dims, 4,
                                                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &in) != nullptr)
        {
            if (error != nullptr) *error = "no se pudo crear el tensor de entrada";
            return false;
        }
        OrtValue* out = nullptr;
        const char* inNames[] = { inputName.toRawUTF8() };
        const char* outNames[] = { outputName.toRawUTF8() };
        auto* st = api->Run(session, nullptr, inNames, (const OrtValue* const*) &in,
                            1, outNames, 1, &out);
        api->ReleaseValue(in);
        if (st != nullptr)
        {
            if (error != nullptr)
                *error = juce::String::fromUTF8(api->GetErrorMessage(st));
            api->ReleaseStatus(st);
            return false;
        }
        float* outData = nullptr;
        if (api->GetTensorMutableData(out, (void**) &outData) != nullptr)
        {
            api->ReleaseValue(out);
            if (error != nullptr) *error = "no se pudo leer la salida del modelo";
            return false;
        }
        std::memcpy(chunkOut.data(), outData, chunkOut.size() * sizeof(float));
        api->ReleaseValue(out); // a partir de acá se lee solo chunkOut

        // 3. Promedio de chunks solapados en el dominio del espectro: los
        //    primeros 128 frames de este chunk ya recibieron la contribución
        //    de la segunda mitad del chunk anterior (win2), así que quedan
        //    definitivos y se sintetizan ya.
        ensureOutCapacity(start + (int64_t) kHalf * kHop + kFft - accStart);
        for (int g = 0; g < kHalf; ++g)
        {
            const float wCur = chunkWindow[(size_t) g];
            const float denom = wCur + (hasPrev ? chunkWindow[(size_t)(kHalf + g)] : 0.0f);
            const float invDenom = denom > 1.0e-8f ? 1.0f / denom : 0.0f;
            const int64_t absPos = start + (int64_t) g * kHop;
            const int64_t off = absPos - accStart;
            for (int ch = 0; ch < 2; ++ch)
            {
                // canales del espectro: L.re=0, R.re=1, L.im=2, R.im=3
                const int chRe = ch;
                const int chIm = ch + 2;
                for (int b = 0; b < kBins; ++b)
                {
                    const size_t idxCur = ((size_t)(chRe * kBins + b)) * kFrames + (size_t) g;
                    const size_t idxCurIm = ((size_t)(chIm * kBins + b)) * kFrames + (size_t) g;
                    const size_t idxPrev = ((size_t)(chRe * kHalf + g)) * kBins + (size_t) b;
                    const size_t idxPrevIm = ((size_t)(chIm * kHalf + g)) * kBins + (size_t) b;
                    frameSpec[(size_t) b].r =
                        (prevSpecW[idxPrev] + chunkOut[idxCur] * wCur) * invDenom;
                    frameSpec[(size_t) b].i =
                        (prevSpecW[idxPrevIm] + chunkOut[idxCurIm] * wCur) * invDenom;
                }
                frameSpec[(size_t) kBins].r = 0.0f; // Nyquist descartado por el modelo
                frameSpec[(size_t) kBins].i = 0.0f;
                float* dst = (ch == 0) ? frameTimeL.data() : frameTimeR.data();
                kiss_fftri(ifftCfg, frameSpec.data(), dst);
                std::vector<float>& acc = (ch == 0) ? vocL : vocR;
                for (int i = 0; i < kFft; ++i)
                    acc[(size_t)(off + i)] += dst[i] * stftWindow[(size_t) i] / (float) kFft;
            }
            for (int i = 0; i < kFft; ++i)
                wsum[(size_t)(off + i)] += stftWindow[(size_t) i] * stftWindow[(size_t) i];
        }

        // 4. Guardar la segunda mitad ponderada para el próximo chunk.
        for (int ch = 0; ch < 4; ++ch)
            for (int g = 0; g < kHalf; ++g)
            {
                const float w = chunkWindow[(size_t)(kHalf + g)];
                for (int b = 0; b < kBins; ++b)
                    prevSpecW[((size_t)(ch * kHalf + g)) * kBins + (size_t) b] =
                        chunkOut[((size_t)(ch * kBins + b)) * kFrames + (size_t)(kHalf + g)] * w;
            }
        hasPrev = true;
        return true;
    }

    void ensureOutCapacity(int64_t needed)
    {
        if ((int64_t) vocL.size() < needed)
        {
            vocL.resize((size_t) needed, 0.0f);
            vocR.resize((size_t) needed, 0.0f);
            wsum.resize((size_t) needed, 0.0f);
        }
    }

    // Recorta prefijos consumidos: entrada antes de outStart (emit puede
    // necesitar el mix de [outStart, chunkStart) para el instrumental) y
    // acumuladores antes de outStart.
    void compact()
    {
        if (outStart > inBufStart)
        {
            const auto drop = (ptrdiff_t) std::min<int64_t>(outStart - inBufStart, (int64_t) inL.size());
            inL.erase(inL.begin(), inL.begin() + drop);
            inR.erase(inR.begin(), inR.begin() + drop);
            inBufStart += drop;
        }
        if (outStart > accStart)
        {
            const auto drop = (ptrdiff_t) std::min<int64_t>(outStart - accStart, (int64_t) vocL.size());
            vocL.erase(vocL.begin(), vocL.begin() + drop);
            vocR.erase(vocR.begin(), vocR.begin() + drop);
            wsum.erase(wsum.begin(), wsum.begin() + drop);
            accStart += drop;
        }
    }

    // Emite hasta outCap muestras finales ([outStart, chunkStart)): vocal =
    // acumulador normalizado por wsum; instrumental = mix - vocal.
    int emit(float* outVocL, float* outVocR, float* outInsL, float* outInsR, int outCap)
    {
        const int64_t avail = chunkStart - outStart;
        const int n = (int) std::min<int64_t>(avail, (int64_t) outCap);
        if (n <= 0)
        {
            compact();
            return 0;
        }
        for (int i = 0; i < n; ++i)
        {
            const int64_t acc = outStart + i - accStart;
            const float w = wsum[(size_t) acc];
            const float norm = 1.0f / std::max(w, 1.0e-8f);
            const float vL = vocL[(size_t) acc] * norm;
            const float vR = vocR[(size_t) acc] * norm;
            const int64_t ip = outStart + i - inBufStart;
            const float mL = (ip >= 0 && ip < (int64_t) inL.size()) ? inL[(size_t) ip] : 0.0f;
            const float mR = (ip >= 0 && ip < (int64_t) inR.size()) ? inR[(size_t) ip] : 0.0f;
            outVocL[i] = vL;
            outVocR[i] = vR;
            outInsL[i] = mL - vL;
            outInsR[i] = mR - vR;
        }
        outStart += n;
        compact();
        return n;
    }
};

TwoStemEngine::TwoStemEngine() : impl(std::make_unique<Impl>()) {}
TwoStemEngine::~TwoStemEngine() = default;

bool TwoStemEngine::isInitialised() const
{
    return impl->session != nullptr;
}

void TwoStemEngine::shutdown()
{
    auto* api = ortApi();
    if (impl->session != nullptr) { api->ReleaseSession(impl->session); impl->session = nullptr; }
    if (impl->options != nullptr) { api->ReleaseSessionOptions(impl->options); impl->options = nullptr; }
    if (impl->env != nullptr) { api->ReleaseEnv(impl->env); impl->env = nullptr; }
    if (impl->memInfo != nullptr) { api->ReleaseMemoryInfo(impl->memInfo); impl->memInfo = nullptr; }
    if (impl->fftCfg != nullptr) { kiss_fft_free(impl->fftCfg); impl->fftCfg = nullptr; }
    if (impl->ifftCfg != nullptr) { kiss_fft_free(impl->ifftCfg); impl->ifftCfg = nullptr; }
}

bool TwoStemEngine::init(juce::String* error)
{
    if (impl->session != nullptr)
        return true;

    const auto model = modelFile();
    if (!model.existsAsFile())
    {
        if (error != nullptr)
            *error = "modelo 2-stem no incluido en esta build";
        return false;
    }

    auto* api = ortApi();
    ORT_CHECK(api->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "DiscNativeStems2", &impl->env), error);
    ORT_CHECK(api->CreateSessionOptions(&impl->options), error);

    const int hw = (int) std::thread::hardware_concurrency();
    (void) api->SetIntraOpNumThreads(impl->options, hw > 4 ? hw - 2 : 2);
    (void) api->SetInterOpNumThreads(impl->options, 1);

    // CoreML EP con la config EXACTA del Gate 0 (17x realtime): MLProgram +
    // CPUAndNeuralEngine + cache. RequireStaticInputShapes se descartó — con
    // él el modelo MDX cae a CPU (~4.4 s/llamada). Fallback silencioso a CPU.
    // El cache va en ~/Library/Caches: el EP de CoreML se rompe con espacios
    // en el path del cache (Application Support los tiene).
    {
        const auto cacheRoot = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                                   .getChildFile("Library/Caches/DiscNativePro/coreml_cache");
        const auto cacheDir = cacheRoot.getChildFile(model.getFileNameWithoutExtension()
                                                     + "-" + juce::String(model.getSize())
                                                     + "-cm");
        cacheDir.createDirectory();
        const auto cachePath = cacheDir.getFullPathName();
        const char* keys[] = { "MLComputeUnits", "ModelFormat", "ModelCacheDirectory" };
        const char* vals[] = { "CPUAndNeuralEngine", "MLProgram", cachePath.toRawUTF8() };
        auto* st = api->SessionOptionsAppendExecutionProvider(impl->options, "CoreML", keys, vals, 3);
        if (st != nullptr)
        {
            std::fprintf(stderr, "stems2: CoreML append fallo: %s\n",
                         api->GetErrorMessage(st));
            api->ReleaseStatus(st);
        }
    }

    // ORTCHAR_T es wchar_t en Windows y char en macOS/Linux: convertir según plataforma.
#if JUCE_WINDOWS
    const juce::CharPointer_UTF16 modelPathUtf16 (model.getFullPathName().toUTF16());
    ORT_CHECK(api->CreateSession(impl->env, reinterpret_cast<const ORTCHAR_T*> (modelPathUtf16.getAddress()),
                                 impl->options, &impl->session), error);
#else
    ORT_CHECK(api->CreateSession(impl->env, reinterpret_cast<const ORTCHAR_T*> (model.getFullPathName().toUTF8().getAddress()),
                                 impl->options, &impl->session), error);
#endif
    ORT_CHECK(api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &impl->memInfo), error);

    OrtAllocator* allocator = nullptr;
    ORT_CHECK(api->GetAllocatorWithDefaultOptions(&allocator), error);
    char* inName = nullptr;
    char* outName = nullptr;
    ORT_CHECK(api->SessionGetInputName(impl->session, 0, allocator, &inName), error);
    ORT_CHECK(api->SessionGetOutputName(impl->session, 0, allocator, &outName), error);
    impl->inputName = juce::String::fromUTF8(inName);
    impl->outputName = juce::String::fromUTF8(outName);
    (void) api->AllocatorFree(allocator, inName);
    (void) api->AllocatorFree(allocator, outName);

    impl->fftCfg = kiss_fftr_alloc(kFft, 0, nullptr, nullptr);
    impl->ifftCfg = kiss_fftr_alloc(kFft, 1, nullptr, nullptr);
    if (impl->fftCfg == nullptr || impl->ifftCfg == nullptr)
    {
        if (error != nullptr) *error = "no se pudo inicializar KissFFT";
        return false;
    }

    impl->stftWindow.resize(kFft);
    for (int i = 0; i < kFft; ++i)
        impl->stftWindow[(size_t) i] = (float) (0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * i / (kFft - 1))));
    impl->chunkWindow.resize(kFrames);
    for (int i = 0; i < kFrames; ++i)
        impl->chunkWindow[(size_t) i] = (float) (0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * i / kFrames))) + 1.0e-8f;

    impl->chunkIn.assign(kSpecElems, 0.0f);
    impl->chunkOut.assign(kSpecElems, 0.0f);
    impl->prevSpecW.assign((size_t) 4 * kHalf * kBins, 0.0f);
    impl->frameSpec.resize(kFft / 2 + 1);
    impl->frameL.resize(kFft);
    impl->frameR.resize(kFft);
    impl->frameTimeL.resize(kFft);
    impl->frameTimeR.resize(kFft);
    return true;
}

int TwoStemEngine::process(const float* inL, const float* inR, int numIn,
                           float* vocL, float* vocR, float* insL, float* insR, int outCap)
{
    if (!isInitialised() || numIn <= 0)
        return 0;

    impl->inL.insert(impl->inL.end(), inL, inL + numIn);
    impl->inR.insert(impl->inR.end(), inR, inR + numIn);
    impl->absIn += numIn;

    juce::String err;
    // Mientras haya entrada suficiente para un chunk completo (los frames se
    // extienden kNeed más allá de chunkStart).
    while (impl->absIn - impl->chunkStart >= kNeed)
    {
        if (!impl->runChunkAt(impl->chunkStart, &err))
            return 0; // el acumulador queda; reintentar es inofensivo
        impl->chunkStart += kAdvance;
    }

    return impl->emit(vocL, vocR, insL, insR, outCap);
}

int TwoStemEngine::flush(float* vocL, float* vocR, float* insL, float* insR, int outCap)
{
    if (impl->flushDone || !isInitialised())
        return impl->emit(vocL, vocR, insL, insR, outCap);

    // Pad con ceros hasta cubrir los frames del último chunk; las muestras
    // más allá del final de la pista se emiten también y el que llama
    // recorta. El avance es kAdvance (igual que el stream) — el target se
    // captura ANTES de padear para que el bucle termine.
    juce::String err;
    const int64_t target = impl->absIn;
    while (impl->chunkStart < target)
    {
        const int64_t need = impl->chunkStart + kNeed - impl->absIn;
        if (need > 0)
        {
            impl->inL.resize(impl->inL.size() + (size_t) need, 0.0f);
            impl->inR.resize(impl->inR.size() + (size_t) need, 0.0f);
            impl->absIn += need;
        }
        if (!impl->runChunkAt(impl->chunkStart, &err))
            break;
        impl->chunkStart += kAdvance;
    }
    impl->flushDone = true;
    return impl->emit(vocL, vocR, insL, insR, outCap);
}

void TwoStemEngine::reset()
{
    impl->inL.clear(); impl->inR.clear();
    impl->vocL.clear(); impl->vocR.clear(); impl->wsum.clear();
    std::fill(impl->prevSpecW.begin(), impl->prevSpecW.end(), 0.0f);
    impl->hasPrev = false;
    impl->flushDone = false;
    impl->absIn = 0; impl->inBufStart = 0;
    impl->chunkStart = 0; impl->outStart = 0; impl->accStart = 0;
}
