#include "OnnxStemEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <onnxruntime_c_api.h>
#include <cmath>
#include <thread>
#include <vector>

namespace
{
constexpr int kSampleRate = 44100;
constexpr int64_t kChunk = 343980;      // segmento de entrenamiento htdemucs (39/5 s)
constexpr int64_t kStep = kChunk / 2;   // solape 50 %
constexpr int64_t kFade = kChunk / 10;  // ventana de fade en los bordes (StemKit)

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

// Ventana de fade suave (coseno elevado) con meseta central.
std::vector<float> fadeWindow(int64_t length)
{
    std::vector<float> w((size_t)length, 1.0f);
    for (int64_t i = 0; i < kFade && i < length; ++i)
    {
        const float t = (float)i / (float)kFade;
        w[(size_t)i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * t);
        w[(size_t)(length - 1 - i)] = w[(size_t)i];
    }
    return w;
}
} // namespace

juce::File OnnxStemEngine::modelFile()
{
    static const juce::String name = "htdemucs_fp32.onnx";

    auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    // .app bundle: Contents/Resources/models/
    auto bundleModel = exe.getParentDirectory().getParentDirectory()
                           .getChildFile("Resources/models").getChildFile(name);
    if (bundleModel.existsAsFile())
        return bundleModel;

    // Windows/Linux: <exe dir>/models/
    auto exeModel = exe.getParentDirectory().getChildFile("models").getChildFile(name);
    if (exeModel.existsAsFile())
        return exeModel;

    // Fallback: instalado por la app en App Support.
    auto appModel = appSupportDir().getChildFile(name);
    if (appModel.existsAsFile())
        return appModel;

    return {};
}

bool OnnxStemEngine::isModelAvailable()
{
    return modelFile().existsAsFile();
}

struct OnnxStemEngine::Impl
{
    OrtEnv* env = nullptr;
    OrtSession* session = nullptr;
    OrtSessionOptions* options = nullptr;
    juce::String inputName = "mix";
    juce::String outputName;

    // Los objetos ORT NO se liberan aquí: el único OnnxStemEngine vive toda
    // la vida de la app (miembro del singleton StemSeparationService) y, al
    // destruirse durante el teardown estático de exit(), ReleaseEnv hace
    // abort() dentro de Environment::~Environment (crash al cerrar la app).
    // El proceso reclama todo al salir.
    ~Impl() = default;
};

OnnxStemEngine::OnnxStemEngine() : impl(std::make_unique<Impl>()) {}
OnnxStemEngine::~OnnxStemEngine() = default;

bool OnnxStemEngine::isInitialised() const
{
    return impl->session != nullptr;
}

void OnnxStemEngine::shutdown()
{
    if (impl->session == nullptr && impl->env == nullptr)
        return;

    auto* api = ortApi();
    if (impl->session != nullptr)
    {
        api->ReleaseSession(impl->session);
        impl->session = nullptr;
    }
    if (impl->options != nullptr)
    {
        api->ReleaseSessionOptions(impl->options);
        impl->options = nullptr;
    }
    if (impl->env != nullptr)
    {
        api->ReleaseEnv(impl->env);
        impl->env = nullptr;
    }
}

bool OnnxStemEngine::init(juce::String* error, bool preferCoreML)
{
    if (impl->session != nullptr)
        return true;

    const auto model = modelFile();
    if (!model.existsAsFile())
    {
        if (error != nullptr)
            *error = "modelo no incluido en esta build";
        return false;
    }

    auto* api = ortApi();
    ORT_CHECK(api->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "DiscNativeStems", &impl->env), error);
    ORT_CHECK(api->CreateSessionOptions(&impl->options), error);

    // Deja 2 cores libres al hilo de audio.
    const int hw = (int)std::thread::hardware_concurrency();
    (void)api->SetIntraOpNumThreads(impl->options, hw > 4 ? hw - 2 : 2);
    (void)api->SetInterOpNumThreads(impl->options, 1);
    // Inferencia bit a bit reproducible entre corridas (re-separación de una
    // pista debe dar los mismos stems).
    (void)api->AddSessionConfigEntry(impl->options, "session.deterministic_compute", "1");

    // P24: CoreML EP con fallback silencioso a CPU. Config ganadora en banco
    // (5 chunks, tiempo de pared): MLProgram + shapes estáticas + CPUAndNeuralEngine
    // = 1.34x vs CPU puro, salida bit-idéntica. "ALL" (con GPU) es 2x MÁS LENTO
    // que CPU; la conversión NeuralNetwork (default) produce NaN con htdemucs;
    // MLComputeUnits es case-sensitive ("ALL"/"CPUOnly"/"CPUAndGPU"/
    // "CPUAndNeuralEngine") y un valor inválido invalida TODO el append.
    // ModelCacheDirectory persistente: convertir el grafo a CoreML es caro (~1 min
    // la primera vez, ~600 MB) y se hace una sola vez; el sufijo del dir distingue
    // la config (una conversión no vale para otra MLComputeUnits).
    if (preferCoreML)
    {
        // El cache va en ~/Library/Caches: el EP de CoreML se rompe con
        // espacios en el path del cache (Application Support los tiene).
        const auto cacheRoot = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                                   .getChildFile("Library/Caches/DiscNativePro/coreml_cache");
        const auto cacheDir = cacheRoot.getChildFile(model.getFileNameWithoutExtension()
                                                     + "-" + juce::String(model.getSize())
                                                     + "-cane");
        cacheDir.createDirectory();
        const auto cachePath = cacheDir.getFullPathName();
        const char* keys[] = { "MLComputeUnits", "ModelFormat",
                               "RequireStaticInputShapes", "ModelCacheDirectory" };
        const char* vals[] = { "CPUAndNeuralEngine", "MLProgram", "1", cachePath.toRawUTF8() };
        auto* st = api->SessionOptionsAppendExecutionProvider(impl->options, "CoreML", keys, vals, 4);
        if (st != nullptr)
            api->ReleaseStatus(st); // sin CoreML (o no soportado): queda el camino CPU
    }

    ORT_CHECK(api->CreateSession(impl->env, model.getFullPathName().toUTF8(),
                                 impl->options, &impl->session), error);

    // Nombre de la salida (estable en el grafo exportado).
    OrtAllocator* allocator = nullptr;
    ORT_CHECK(api->GetAllocatorWithDefaultOptions(&allocator), error);
    char* outName = nullptr;
    ORT_CHECK(api->SessionGetOutputName(impl->session, 0, allocator, &outName), error);
    impl->outputName = juce::String::fromUTF8(outName);
    (void)api->AllocatorFree(allocator, outName);

    return true;
}

bool OnnxStemEngine::separate(const juce::File& audioFile, const juce::File& outDir,
                              const CancelFn& shouldCancel)
{
    juce::String err;
    if (!init(&err))
        return false;
    auto cancelled = [&shouldCancel]() { return shouldCancel && shouldCancel(); };
    if (cancelled())
        return false;

    // ---------- 1. Decodificar a estéreo float ----------
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(audioFile));
    if (reader == nullptr)
        return false;

    const auto srcRate = reader->sampleRate;
    const auto srcFrames = reader->lengthInSamples;
    if (srcFrames <= 0)
        return false;

    // Margen para el lookahead del interpolador (evita leer fuera del buffer).
    const auto paddedFrames = srcFrames + 16;
    juce::AudioBuffer<float> src;
    src.setSize(2, (int)paddedFrames, false, false, true);
    src.clear();
    reader->read(&src, 0, (int)srcFrames, 0, true, true);
    reader.reset();

    // ---------- 2. Resamplear a 44.1 kHz si hace falta ----------
    const double ratio = (double)srcRate / (double)kSampleRate;
    std::vector<float> chan[2];
    if (std::abs(ratio - 1.0) < 1e-9)
    {
        for (int c = 0; c < 2; ++c)
        {
            const auto* rd = src.getReadPointer(c);
            chan[c].assign(rd, rd + srcFrames);
        }
    }
    else
    {
        const auto outFrames = (juce::int64)std::ceil((double)srcFrames * kSampleRate / (double)srcRate);
        for (int c = 0; c < 2; ++c)
        {
            juce::LagrangeInterpolator interp;
            chan[c].resize((size_t)outFrames, 0.0f);
            interp.process(ratio, src.getReadPointer(c), chan[c].data(), (int)outFrames);
        }
    }
    src.setSize(0, 0); // libera la copia pre-resample antes del bucle de chunks
    const int64_t total = (int64_t)chan[0].size();
    if (cancelled() || total <= 0)
        return false;

    // ---------- 3. Chunks con solape y fade ----------
    const auto win = fadeWindow(kChunk);
    std::vector<float> stems[4][2]; // drums, bass, other, vocals x canal
    std::vector<float> wsum((size_t)total, 0.0f);
    for (auto& s0 : stems)
        for (auto& s1 : s0)
            s1.assign((size_t)total, 0.0f);

    auto* api = ortApi();
    const int64_t dims[3] = { 1, 2, kChunk };
    std::vector<float> chunkIn((size_t)(2 * kChunk), 0.0f);
    OrtMemoryInfo* memInfo = nullptr;
    ORT_CHECK(api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memInfo), &err);

    auto reflectIndex = [&](int64_t i)
    {
        // Reflexión en los bordes para que todos los chunks estén llenos.
        if (total == 1)
            return (int64_t)0;
        const int64_t period = 2 * (total - 1);
        i = ((i % period) + period) % period;
        if (i >= total)
            i = period - i;
        return i;
    };

    bool failed = false;
    // Chunks cada kStep; los que desbordan la pista leen muestras reflejadas.
    for (int64_t pos = 0; pos < total && !failed; pos += kStep)
    {
        for (int64_t i = 0; i < kChunk; ++i)
        {
            const auto idx = reflectIndex(pos + i);
            chunkIn[(size_t)i] = chan[0][(size_t)idx];
            chunkIn[(size_t)(kChunk + i)] = chan[1][(size_t)idx];
        }

        OrtValue* in = nullptr;
        if (api->CreateTensorWithDataAsOrtValue(memInfo, chunkIn.data(),
                                                chunkIn.size() * sizeof(float),
                                                dims, 3,
                                                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &in) != nullptr)
        {
            failed = true;
            break;
        }

        OrtValue* out = nullptr;
        const char* inNames[] = { impl->inputName.toRawUTF8() };
        const char* outNames[] = { impl->outputName.toRawUTF8() };
        if (api->Run(impl->session, nullptr, inNames,
                     (const OrtValue* const*)&in, 1, outNames, 1, &out) != nullptr)
        {
            api->ReleaseValue(in);
            failed = true;
            break;
        }
        api->ReleaseValue(in);

        float* outData = nullptr;
        if (api->GetTensorMutableData(out, (void**)&outData) != nullptr)
        {
            api->ReleaseValue(out);
            failed = true;
            break;
        }

        // outData: [1, 4, 2, kChunk] — drums, bass, other, vocals (orden de fuentes htdemucs)
        for (int s = 0; s < 4; ++s)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float* rd = outData + ((size_t)s * 2 + (size_t)c) * kChunk;
                auto& dst = stems[s][c];
                for (int64_t i = 0; i < kChunk; ++i)
                {
                    const int64_t gi = pos + i;
                    if (gi >= total)
                        break;
                    dst[(size_t)gi] += rd[i] * win[(size_t)i];
                }
            }
        }
        for (int64_t i = 0; i < kChunk; ++i)
        {
            const int64_t gi = pos + i;
            if (gi >= total)
                break;
            wsum[(size_t)gi] += win[(size_t)i];
        }

        api->ReleaseValue(out);

        if (cancelled())
        {
            failed = true;
            break;
        }
    }

    api->ReleaseMemoryInfo(memInfo);
    if (failed)
        return false;

    // ---------- 4. Normalizar por la suma de ventanas y escribir WAVs ----------
    static const char* names[] = { "drums", "bass", "other", "vocals" };
    outDir.createDirectory();
    juce::WavAudioFormat wavFormat;

    for (int s = 0; s < 4; ++s)
    {
        auto file = outDir.getChildFile(juce::String(names[s]) + ".wav");
        auto stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(stream.release(), (double)kSampleRate, 2, 16, {}, 0));
        if (writer == nullptr)
            return false;

        juce::AudioBuffer<float> buf(2, (int)juce::jmin(total, (int64_t)1 << 20));
        for (int64_t gi = 0; gi < total; gi += buf.getNumSamples())
        {
            const int n = (int)juce::jmin(total - gi, (int64_t)buf.getNumSamples());
            for (int c = 0; c < 2; ++c)
            {
                auto* wr = buf.getWritePointer(c);
                for (int i = 0; i < n; ++i)
                {
                    const auto idx = (size_t)(gi + i);
                    const float inv = 1.0f / juce::jmax(wsum[idx], 1e-8f);
                    wr[i] = stems[s][c][idx] * inv;
                }
            }
            writer->writeFromAudioSampleBuffer(buf, 0, n);
        }
        writer.reset();
    }

    return true;
}
