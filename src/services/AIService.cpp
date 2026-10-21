#include "AIService.h"
#include "YouTubeService.h"
#include "SecretStore.h"
#include "LanguageManager.h"
#include <juce_events/juce_events.h>
#include <thread>

juce::File AIService::getSettingsFile()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DiscNativePro");
    if (!appData.exists())
        appData.createDirectory();
    return appData.getChildFile("settings.json");
}

AIProvider AIService::getProvider()
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("aiProvider"))
        {
            auto p = json["aiProvider"].toString().toLowerCase();
            if (p == "openai") return AIProvider::OpenAI;
            if (p == "claude") return AIProvider::Claude;
            if (p == "deepseek") return AIProvider::DeepSeek;
            if (p == "openrouter") return AIProvider::OpenRouter;
            return AIProvider::Gemini;
        }
    }
    return AIProvider::Gemini;
}

void AIService::setProvider(AIProvider provider)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        obj->setProperty("aiProvider", getProviderKeyName(provider));
    }
    file.replaceWithText(juce::JSON::toString(json));
}

juce::String AIService::getProviderName(AIProvider provider)
{
    switch (provider)
    {
        case AIProvider::Gemini:   return "Google Gemini (Flash)";
        case AIProvider::OpenAI:   return "OpenAI (ChatGPT)";
        case AIProvider::Claude:   return "Anthropic Claude";
        case AIProvider::DeepSeek: return "DeepSeek";
        case AIProvider::OpenRouter: return "OpenRouter (200+ modelos)";
    }
    return "Google Gemini";
}

juce::String AIService::getProviderKeyName(AIProvider provider)
{
    switch (provider)
    {
        case AIProvider::Gemini:   return "gemini";
        case AIProvider::OpenAI:   return "openai";
        case AIProvider::Claude:   return "claude";
        case AIProvider::DeepSeek: return "deepseek";
        case AIProvider::OpenRouter: return "openrouter";
    }
    return "gemini";
}

bool AIService::isValidModelName(const juce::String& model)
{
    if (model.isEmpty())
        return false;
    for (auto c : model)
    {
        const auto ch = (juce::juce_wchar) c;
        const bool allowed = juce::CharacterFunctions::isLetterOrDigit(ch)
                             || ch == '-' || ch == '_' || ch == '.' || ch == '/' || ch == ':';
        if (!allowed)
            return false;
    }
    return true;
}

juce::String AIService::getDefaultModel(AIProvider provider)
{
    switch (provider)
    {
        case AIProvider::Gemini:   return "gemini-3.8-flash";
        case AIProvider::OpenAI:   return "gpt-4o-mini";
        case AIProvider::Claude:   return "claude-3-5-haiku-20241022";
        case AIProvider::DeepSeek: return "deepseek-chat";
        case AIProvider::OpenRouter: return "google/gemini-2.5-flash";
    }
    return "gemini-3.8-flash";
}

juce::StringArray AIService::getAvailableModels(AIProvider provider)
{
    switch (provider)
    {
        case AIProvider::Gemini:
            return { "gemini-3.8-flash", "gemini-3.6-flash", "gemini-2.5-flash" };
        case AIProvider::OpenAI:
            return { "gpt-4o-mini", "gpt-4o" };
        case AIProvider::Claude:
            return { "claude-3-5-haiku-20241022", "claude-3-5-sonnet-20241022" };
        case AIProvider::DeepSeek:
            return { "deepseek-chat", "deepseek-reasoner" };
        case AIProvider::OpenRouter:
            return { "google/gemini-2.5-flash", "anthropic/claude-3.5-haiku", "deepseek/deepseek-chat",
                     "openai/gpt-4o-mini", "meta-llama/llama-3.3-70b-instruct", "x-ai/grok-3-mini" };
    }
    return { getDefaultModel(provider) };
}

juce::String AIService::getModel(AIProvider provider)
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject())
        {
            juce::Identifier propId("aiModel_" + getProviderKeyName(provider));
            if (json.hasProperty(propId))
            {
                auto m = json[propId].toString().trim();
                if (m.isNotEmpty())
                    return m;
            }
        }
    }
    return getDefaultModel(provider);
}

void AIService::setModel(AIProvider provider, const juce::String& model)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        juce::Identifier propId("aiModel_" + getProviderKeyName(provider));
        auto m = model.trim();
        if (m.isNotEmpty())
            obj->setProperty(propId, m);
        else
            obj->removeProperty(propId);
    }
    file.replaceWithText(juce::JSON::toString(json));
}

juce::String AIService::getApiKey(AIProvider provider)
{
    const auto account = "ai/" + getProviderKeyName(provider);
    const auto secureValue = SecretStore::read(account);
    if (secureValue.isNotEmpty())
        return secureValue.trim();

    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject())
        {
            juce::Identifier propId("aiApiKey_" + getProviderKeyName(provider));
            if (json.hasProperty(propId))
            {
                auto legacyValue = json[propId].toString().trim();
                if (legacyValue.isNotEmpty() && SecretStore::write(account, legacyValue))
                {
                    if (auto* obj = json.getDynamicObject())
                    {
                        obj->removeProperty(propId);
                        file.replaceWithText(juce::JSON::toString(json));
                    }
                }
                return legacyValue;
            }
            // Fallback to generic aiApiKey if it's the current provider
            if (json.hasProperty("aiApiKey") && getProvider() == provider)
            {
                auto legacyValue = json["aiApiKey"].toString().trim();
                if (legacyValue.isNotEmpty() && SecretStore::write(account, legacyValue))
                {
                    if (auto* obj = json.getDynamicObject())
                    {
                        obj->removeProperty("aiApiKey");
                        file.replaceWithText(juce::JSON::toString(json));
                    }
                }
                return legacyValue;
            }
        }
    }
    return {};
}

void AIService::setApiKey(AIProvider provider, const juce::String& key)
{
    const auto account = "ai/" + getProviderKeyName(provider);
    const auto trimmedKey = key.trim();
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        juce::Identifier propId("aiApiKey_" + getProviderKeyName(provider));
        if (trimmedKey.isNotEmpty())
        {
            if (SecretStore::write(account, trimmedKey))
                obj->removeProperty(propId);
            else
                obj->setProperty(propId, trimmedKey);
        }
        else
        {
            SecretStore::remove(account);
            obj->removeProperty(propId);
            obj->removeProperty("aiApiKey");
        }
    }
    file.replaceWithText(juce::JSON::toString(json));
}

void AIService::migrateLegacySecrets()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile())
        return;

    auto json = juce::JSON::parse(file);
    if (!json.isObject())
        return;

    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
        return;

    const AIProvider allProviders[] { AIProvider::Gemini, AIProvider::OpenAI, AIProvider::Claude,
                                      AIProvider::DeepSeek, AIProvider::OpenRouter };

    bool changed = false;
    for (auto provider : allProviders)
    {
        const auto account = juce::String("ai/") + getProviderKeyName(provider);
        const auto secure = SecretStore::read(account).trim();

        juce::Identifier propId("aiApiKey_" + getProviderKeyName(provider));
        const auto legacy = json.hasProperty(propId) ? json[propId].toString().trim() : juce::String();

        // Elimina la key en claro solo si el almacén seguro ya la tiene o la escritura tuvo éxito.
        if (legacy.isNotEmpty())
        {
            if (secure.isNotEmpty() || SecretStore::write(account, legacy))
            {
                obj->removeProperty(propId);
                changed = true;
            }
        }
        else if (json.hasProperty(propId))
        {
            obj->removeProperty(propId);
            changed = true;
        }
    }

    // Key genérica legacy: se asigna al proveedor actual.
    if (json.hasProperty("aiApiKey"))
    {
        const auto account = juce::String("ai/") + getProviderKeyName(getProvider());
        const auto legacy = json["aiApiKey"].toString().trim();
        if (legacy.isEmpty()
            || (SecretStore::read(account).isNotEmpty() || SecretStore::write(account, legacy)))
        {
            obj->removeProperty("aiApiKey");
            changed = true;
        }
    }

    if (changed)
        file.replaceWithText(juce::JSON::toString(json));
}

bool AIService::isEnabled()
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("aiEnabled"))
        {
            return (bool)json["aiEnabled"];
        }
    }
    // Default to enabled if an API key is already configured
    return isConfigured();
}

void AIService::setEnabled(bool enabled)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        obj->setProperty("aiEnabled", enabled);
    }
    file.replaceWithText(juce::JSON::toString(json));
}

bool AIService::isConfigured()
{
    return getApiKey(getProvider()).isNotEmpty();
}

juce::String AIService::buildPrompt(const CurrentTrackContext& context)
{
    juce::String prompt;
    prompt << juce::String::fromUTF8("Actúa como un DJ experto que conoce qué pone el público junto a cada canción en festivales y clubes.\n");
    prompt << juce::String::fromUTF8("La pista en reproducción es:\n");
    prompt << juce::String::fromUTF8("- Título: \"") << (context.title.isNotEmpty() ? context.title : "desconocido") << "\"\n";
    if (context.artist.isNotEmpty())
        prompt << "- Artista: \"" << context.artist << "\"\n";
    if (context.bpm > 0.0)
        prompt << "- BPM Detectado: " << juce::String(context.bpm, 1) << "\n";
    if (context.key.isNotEmpty() && context.key != "--" && context.key != "YT")
        prompt << "- Tonalidad (Camelot): " << context.key << "\n";
    if (!context.libraryCandidates.empty())
    {
        prompt << "\n" << juce::String::fromUTF8("El DJ tiene las siguientes canciones en su biblioteca local disponible:\n");
        for (const auto& c : context.libraryCandidates)
        {
            prompt << "[" << juce::String(c.index) << "] \"" << c.title << "\" - " << c.artist;
            if (c.bpm > 0.0)
                prompt << " (BPM: " << juce::String(c.bpm, 1) << ")";
            if (c.key.isNotEmpty() && c.key != "--")
                prompt << " [Key: " << c.key << "]";
            if (c.folder.isNotEmpty() && c.folder != "Todas las Pistas")
                prompt << " {Carpeta: " << c.folder << "}";
            prompt << "\n";
        }
        prompt << "\n";
        prompt << juce::String::fromUTF8("INSTRUCCIÓN CRÍTICA Y OBLIGATORIA:\n");
        prompt << juce::String::fromUTF8("Debes armar la mejor sesión de AutoMix seleccionando ÚNICAMENTE canciones de la lista de canciones disponibles en la biblioteca local del DJ de arriba.\n");
        prompt << juce::String::fromUTF8("Elige hasta 10 canciones de esa lista (o todas las que consideres coherentes si hay menos de 10) ")
               << juce::String::fromUTF8("y ordénalas en la secuencia perfecta para mezclar de manera fluida.\n");
        prompt << juce::String::fromUTF8("Criterios de selección y orden:\n");
        prompt << juce::String::fromUTF8("1. Mismo artista o mismo estilo/género/carpeta.\n");
        prompt << juce::String::fromUTF8("2. Tempo (BPM) compatible y transición armónica (Camelot: mismo tono, relativo o +/-1).\n");
        prompt << juce::String::fromUTF8("   REGLA DE HALF-TIME / DOUBLE-TIME: Considera plenamente compatibles los tempos a mitad o doble (ej. 70-75 BPM de Trap/Dubstep/Reggaeton equivale y mezcla perfecto con 140-150 BPM). En un set con energía ascendente, ubica los temas de half-time en su tempo efectivo doble (~140-150 BPM) según su impacto y drop, NUNCA los envíes al inicio de la sesión como si fueran canciones lentas.\n");
        prompt << juce::String::fromUTF8("3. Mantén o sube la energía de la sesión para el público.\n");
        prompt << juce::String::fromUTF8("En el JSON de respuesta, pon en 'title' y 'artist' EXACTAMENTE los mismos nombres que aparecen en la lista numerada de arriba.\n");
        prompt << juce::String::fromUTF8("PROHIBIDO recomendar canciones que no estén en la lista de disponibles.\n");
    }
    else
    {
        // Fuente + señales de audiencia (lo más importante va primero en el prompt)
        if (context.audienceAlsoPlays.size() > 0)
        {
            prompt << juce::String::fromUTF8("- La audiencia de esta pista también escucha/mezcla: ");
            for (int i = 0; i < context.audienceAlsoPlays.size(); ++i)
            {
                if (i > 0) prompt << " | ";
                prompt << "\"" << context.audienceAlsoPlays[i] << "\"";
            }
            prompt << "\n";
        }
        if (context.videoTags.size() > 0)
        {
            prompt << "- Etiquetas del video: ";
            for (int i = 0; i < context.videoTags.size(); ++i)
            {
                if (i > 0) prompt << ", ";
                prompt << context.videoTags[i];
            }
            prompt << "\n";
        }
        prompt << "\n";

        prompt << juce::String::fromUTF8("Paso 1: identifica el GÉNERO y escena exactos de la pista (ej. Latin Tech, Afro House, Melodic Techno). ")
               << juce::String::fromUTF8("El género manda: toda recomendación debe ser del mismo género o de un estilo hermano que suene en las mismas fiestas.\n");
        prompt << juce::String::fromUTF8("Paso 2: recomienda exactamente 10 canciones que el público y los DJs ponen JUNTO a esta canción ")
               << "(co-ocurrencia real en sets y sesiones, no canciones vagamente parecidas). "
               << juce::String::fromUTF8("Prioriza las de la lista de audiencia de arriba cuando encajen en género y energía.\n");
        prompt << juce::String::fromUTF8("Paso 3: filtra por compatibilidad de mezcla: tempo cercano (+/- 4 BPM del detectado o relación armónica Half/Double-Time) y energía continua (escala 1-10).\n");
        prompt << juce::String::fromUTF8("Si NO reconoces la pista (título genérico, artista desconocido o es muy nueva), pon \"known\": false en TODAS ")
               << juce::String::fromUTF8("y recomienda por género y energía en vez de inventar afinidades.\n");
    }
    prompt << juce::String::fromUTF8("PROHIBIDO markdown, cercas ``` o texto fuera del JSON. "
                                     "Solo el objeto, nada más. Reasons de máximo 120 caracteres.\n");
    prompt << juce::String::fromUTF8("Responde ÚNICAMENTE un objeto JSON válido con la clave 'recommendations', exactamente así:\n")
           << "{\n"
           << "  \"recommendations\": [\n"
           << "    {\n"
           << juce::String::fromUTF8("      \"title\": \"Nombre de la canción\",\n")
           << "      \"artist\": \"Nombre del artista\",\n"
           << "      \"bpm\": 126,\n"
           << "      \"key\": \"8A\",\n"
           << "      \"genre\": \"Tech House\",\n"
           << juce::String::fromUTF8("      \"reason\": \"Suena en los mismos sets de FISHER; mismo groove y energía 8/10 para mantener la pista\",\n")
           << "      \"matchType\": \"crowd\",\n"
           << "      \"energy\": 8,\n"
           << "      \"known\": true\n"
           << "    }\n"
           << "  ]\n"
           << "}\n"
           << juce::String::fromUTF8("\"matchType\" solo admite: \"crowd\" (el público la pone junto a esta), ")
           << juce::String::fromUTF8("\"genre\" (mismo género/escena) o \"harmonic\" (compatible por tempo/tono). Ordena: crowd primero.\n");

    return prompt;
}

std::vector<AIRecommendation> AIService::parseResponse(AIProvider provider, const juce::String& rawJson, juce::String& outError)
{
    juce::ignoreUnused(provider);
    std::vector<AIRecommendation> list;

    auto parsed = juce::JSON::parse(rawJson);
    if (!parsed.isObject())
    {
        outError = LanguageManager::trUtf8("Respuesta no válida del servidor de IA.", "Invalid response from AI server.");
        return list;
    }

    juce::String textContent;
    bool truncated = false;

    // Check Gemini format: candidates[0].content.parts[*].text (todas las partes, no solo la primera)
    if (parsed.hasProperty("candidates"))
    {
        auto candidates = parsed["candidates"];
        if (candidates.isArray() && candidates.size() > 0)
        {
            auto first = candidates[0];
            if (first["finishReason"].toString().toUpperCase() == "MAX_TOKENS")
                truncated = true;
            if (first.hasProperty("content") && first["content"].hasProperty("parts"))
            {
                auto parts = first["content"]["parts"];
                if (parts.isArray())
                {
                    for (int p = 0; p < parts.size(); ++p)
                        textContent += parts[p]["text"].toString();
                }
            }
        }
    }
    // Check OpenAI / DeepSeek format: choices[0].message.content
    else if (parsed.hasProperty("choices"))
    {
        auto choices = parsed["choices"];
        if (choices.isArray() && choices.size() > 0)
        {
            auto first = choices[0];
            if (first["finish_reason"].toString().toLowerCase() == "length")
                truncated = true;
            if (first.hasProperty("message") && first["message"].hasProperty("content"))
            {
                textContent = first["message"]["content"].toString();
            }
        }
    }
    // Check Claude format: content[*].text (concatenar bloques de texto)
    else if (parsed.hasProperty("content"))
    {
        auto content = parsed["content"];
        if (content.isArray())
        {
            for (int c = 0; c < content.size(); ++c)
            {
                if (content[c]["type"].toString() == "text" || content[c].hasProperty("text"))
                    textContent += content[c]["text"].toString();
            }
        }
        if (parsed["stop_reason"].toString().toLowerCase() == "max_tokens")
            truncated = true;
    }
    // Direct recommendations key
    else if (parsed.hasProperty("recommendations"))
    {
        textContent = rawJson;
    }

    if (textContent.isEmpty())
    {
        if (parsed.hasProperty("error"))
        {
            outError = parsed["error"]["message"].toString();
        }
        else
        {
            outError = "No se pudo interpretar el contenido generado por la IA.";
        }
        return list;
    }

    // Strip markdown code fences if present (e.g. ```json ... ```)
    juce::String cleanText = textContent.trim();
    if (cleanText.startsWith("```json"))
    {
        cleanText = cleanText.substring(7);
        if (cleanText.endsWith("```"))
            cleanText = cleanText.dropLastCharacters(3).trim();
    }
    else if (cleanText.startsWith("```"))
    {
        cleanText = cleanText.substring(3);
        if (cleanText.endsWith("```"))
            cleanText = cleanText.dropLastCharacters(3).trim();
    }
    cleanText = cleanText.trim();

    // Salvamento: recortar preámbulos/epílogos (primer '{'/'[' ... último '}'/']')
    {
        int openBrace = cleanText.indexOf("{");
        int openBracket = cleanText.indexOf("[");
        int start = -1;
        juce::String closeCh;
        if (openBrace >= 0 && (openBracket < 0 || openBrace < openBracket))
        {
            start = openBrace;
            closeCh = "}";
        }
        else if (openBracket >= 0)
        {
            start = openBracket;
            closeCh = "]";
        }
        if (start > 0)
            cleanText = cleanText.substring(start).trim();
        int end = (start >= 0) ? cleanText.lastIndexOf(closeCh) : -1;
        if (end > start)
            cleanText = cleanText.substring(0, end + 1).trim();
    }

    auto recsJson = juce::JSON::parse(cleanText);
    juce::var items;
    if (recsJson.isArray())
    {
        items = recsJson; // el modelo devolvió el array pelado
    }
    else if (recsJson.isObject() && recsJson.hasProperty("recommendations"))
    {
        items = recsJson["recommendations"];
    }
    else
    {
        if (truncated)
            outError = LanguageManager::trUtf8("Respuesta truncada por límite de tokens: regenera de nuevo.",
                                               "Response truncated by token limit: regenerate again.");
        else
            outError = LanguageManager::trUtf8("Formato JSON de recomendaciones no reconocido.",
                                               "Unrecognized recommendation JSON format.");
        return list;
    }

    if (!items.isArray())
    {
        outError = LanguageManager::trUtf8("La lista de recomendaciones está vacía.",
                                           "Recommendation list is empty.");
        return list;
    }

    for (int i = 0; i < items.size(); ++i)
    {
        auto it = items[i];
        if (!it.isObject()) continue;

        AIRecommendation rec;
        rec.title  = it["title"].toString();
        rec.artist = it["artist"].toString();
        rec.bpm    = (double)it["bpm"];
        rec.key    = it["key"].toString();
        rec.genre  = it["genre"].toString();
        rec.reason = it["reason"].toString();

        // Campos nuevos con defaults tolerantes (modelos viejos/configs viejas los omiten)
        auto mt = it["matchType"].toString().toLowerCase().trim();
        rec.matchType = (mt == "crowd" || mt == "harmonic") ? mt : "genre";
        rec.energy = juce::jlimit(0, 10, (int)it["energy"]);
        rec.known = it.hasProperty("known") ? (bool)it["known"] : true;

        if (rec.title.isNotEmpty())
            list.push_back(rec);
    }

    return list;
}

void AIService::requestRecommendationsAsync(
    const CurrentTrackContext& context,
    std::function<void(const std::vector<AIRecommendation>&, const juce::String&)> onComplete)
{
    auto provider = getProvider();
    auto apiKey = getApiKey(provider);
    auto modelName = getModel(provider).trim();
    if (!isValidModelName(modelName))
        modelName = getDefaultModel(provider);

    if (apiKey.isEmpty())
    {
        juce::MessageManager::callAsync([onComplete]() {
            onComplete({}, "Clave API no configurada para el proveedor seleccionado.");
        });
        return;
    }

    std::thread([context, provider, apiKey, modelName, onComplete]() {
#if JUCE_DEBUG
        const auto t0 = juce::Time::getMillisecondCounter();
#endif

        // Señal de audiencia en paralelo + caché (ver YouTubeService::getAudienceSignals)
        CurrentTrackContext enriched = context;
        if (enriched.isVideo && enriched.videoId.isNotEmpty())
        {
            auto signals = YouTubeService::getAudienceSignals(enriched.videoId);
            enriched.audienceAlsoPlays = signals.related;
            enriched.videoTags = signals.tags;
        }
        DBG("AI-Match: contexto listo en " << (juce::Time::getMillisecondCounter() - t0) << " ms");

        std::vector<AIRecommendation> recs;
        juce::String parseError;
        bool apiError = false;

        // A toda costa: reintento con auto-reparación si el modelo devuelve
        // formato inválido (cualquier modelo de OpenRouter puede fallar).
        for (int attempt = 0; attempt < 2 && recs.empty() && !apiError; ++attempt)
        {
            juce::String promptText = (attempt == 0)
                ? buildPrompt(enriched)
                : buildPrompt(enriched) + juce::String::fromUTF8("\n\nTu respuesta anterior no fue JSON válido. Responde SOLO el objeto JSON, sin nada más.");
        juce::String endpoint;
        juce::String postData;
        juce::String extraHeaders;

        juce::var payload(new juce::DynamicObject());

        if (provider == AIProvider::Gemini)
        {
            endpoint = "https://generativelanguage.googleapis.com/v1beta/models/" + modelName + ":generateContent";
            extraHeaders = "x-goog-api-key: " + apiKey;

            // Gemini payload
            juce::var partsArray;
            juce::var partObj(new juce::DynamicObject());
            partObj.getDynamicObject()->setProperty("text", promptText);
            partsArray.append(partObj);

            juce::var contentObj(new juce::DynamicObject());
            contentObj.getDynamicObject()->setProperty("parts", partsArray);
            juce::var contentsArray;
            contentsArray.append(contentObj);

            payload.getDynamicObject()->setProperty("contents", contentsArray);

            juce::var genConfig(new juce::DynamicObject());
            genConfig.getDynamicObject()->setProperty("responseMimeType", "application/json");
            genConfig.getDynamicObject()->setProperty("maxOutputTokens", 1600); // 10 recs + margen anti-truncado
            payload.getDynamicObject()->setProperty("generationConfig", genConfig);
        }
        else if (provider == AIProvider::OpenAI)
        {
            endpoint = "https://api.openai.com/v1/chat/completions";
            extraHeaders = "Authorization: Bearer " + apiKey;

            payload.getDynamicObject()->setProperty("model", modelName);

            juce::var messagesArray;
            juce::var msgObj(new juce::DynamicObject());
            msgObj.getDynamicObject()->setProperty("role", "user");
            msgObj.getDynamicObject()->setProperty("content", promptText);
            messagesArray.append(msgObj);

            payload.getDynamicObject()->setProperty("messages", messagesArray);

            juce::var respFmt(new juce::DynamicObject());
            respFmt.getDynamicObject()->setProperty("type", "json_object");
            payload.getDynamicObject()->setProperty("response_format", respFmt);
            payload.getDynamicObject()->setProperty("max_tokens", 1400);
            payload.getDynamicObject()->setProperty("temperature", 0);
        }
        else if (provider == AIProvider::DeepSeek)
        {
            endpoint = "https://api.deepseek.com/chat/completions";
            extraHeaders = "Authorization: Bearer " + apiKey;

            payload.getDynamicObject()->setProperty("model", modelName);

            juce::var messagesArray;
            juce::var msgObj(new juce::DynamicObject());
            msgObj.getDynamicObject()->setProperty("role", "user");
            msgObj.getDynamicObject()->setProperty("content", promptText);
            messagesArray.append(msgObj);

            payload.getDynamicObject()->setProperty("messages", messagesArray);

            juce::var respFmt(new juce::DynamicObject());
            respFmt.getDynamicObject()->setProperty("type", "json_object");
            payload.getDynamicObject()->setProperty("response_format", respFmt);
            payload.getDynamicObject()->setProperty("max_tokens", 1400);
            payload.getDynamicObject()->setProperty("temperature", 0);
        }
        else if (provider == AIProvider::Claude)
        {
            endpoint = "https://api.anthropic.com/v1/messages";
            extraHeaders = "x-api-key: " + apiKey + "\r\nanthropic-version: 2023-06-01";

            payload.getDynamicObject()->setProperty("model", modelName);
            payload.getDynamicObject()->setProperty("max_tokens", 1600);

            juce::var messagesArray;
            juce::var msgObj(new juce::DynamicObject());
            msgObj.getDynamicObject()->setProperty("role", "user");
            msgObj.getDynamicObject()->setProperty("content", promptText);
            messagesArray.append(msgObj);

            payload.getDynamicObject()->setProperty("messages", messagesArray);
        }
        else if (provider == AIProvider::OpenRouter)
        {
            // API compatible con OpenAI; sin response_format (algunos modelos free devuelven 400).
            // El salvamento por llaves de parseResponse cubre el formato.
            endpoint = "https://openrouter.ai/api/v1/chat/completions";
            extraHeaders = "Authorization: Bearer " + apiKey
                         + "\r\nHTTP-Referer: https://discnativepro.app"
                         + "\r\nX-Title: DiscNativePro";

            payload.getDynamicObject()->setProperty("model", modelName);

            juce::var orMessages;
            juce::var orMsg(new juce::DynamicObject());
            orMsg.getDynamicObject()->setProperty("role", "user");
            orMsg.getDynamicObject()->setProperty("content", promptText);
            orMessages.append(orMsg);

            payload.getDynamicObject()->setProperty("messages", orMessages);
            payload.getDynamicObject()->setProperty("max_tokens", 1600);
            payload.getDynamicObject()->setProperty("temperature", 0);
        }

        postData = juce::JSON::toString(payload);

        int statusCode = 0;
        auto request = juce::URL::createWithoutParsing(endpoint).withPOSTData(postData);
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                           .withHttpRequestCmd("POST")
                           .withExtraHeaders("Content-Type: application/json\r\n" + extraHeaders)
                           .withConnectionTimeoutMs(15000)
                           .withNumRedirectsToFollow(3)
                           .withStatusCode(&statusCode);
        auto responseStream = request.createInputStream(options);

        juce::String rawResponse;
        if (responseStream != nullptr)
            rawResponse = responseStream->readEntireStreamAsString();

        if (rawResponse.isEmpty())
        {
            parseError = juce::String::fromUTF8("No hubo respuesta del servidor o tiempo de espera agotado.");
            break;
        }

        {
            // Distinguir error de API (key/créditos/rate-limit: no tiene sentido reintentar)
            // de formato inválido (reintentable con auto-reparación).
            auto envelope = juce::JSON::parse(rawResponse);
            apiError = envelope.isObject()
                && (envelope.hasProperty("error") || envelope["type"].toString() == "error");
            if (apiError)
            {
                parseResponse(provider, rawResponse, parseError); // extrae el mensaje de la API
                break;
            }
        }
        recs = parseResponse(provider, rawResponse, parseError);
        } // for attempt

        if (recs.empty() && enriched.audienceAlsoPlays.size() > 0)
        {
            // Fallback determinístico cero-IA desde Up Next: nunca pantalla roja
            // si hay señal de audiencia (funciona incluso sin key/créditos).
            recs = buildCrowdFallback(enriched);
            parseError = {};
        }
        DBG("AI-Match: total " << (juce::Time::getMillisecondCounter() - t0) << " ms (" << recs.size() << " recs)");

        juce::MessageManager::callAsync([onComplete, recs, parseError]() {
            onComplete(recs, parseError);
        });
    }).detach();
}

std::vector<AIRecommendation> AIService::buildCrowdFallback(const CurrentTrackContext& context)
{
    std::vector<AIRecommendation> list;

    for (int i = 0; i < context.audienceAlsoPlays.size() && (int)list.size() < 10; ++i)
    {
        auto entry = context.audienceAlsoPlays[i].trim();
        if (entry.isEmpty())
            continue;

        AIRecommendation rec;
        int sep = entry.indexOf(juce::String::fromUTF8(" — "));
        if (sep > 0)
        {
            // getRelatedVideos guarda "título — canal"
            rec.title = entry.substring(0, sep).trim();
            rec.artist = entry.substring(sep + 3).trim();
        }
        else
        {
            rec.title = entry;
        }

        if (rec.title.isEmpty())
            continue;

        rec.matchType = "crowd";
        rec.known = true; // picks reales de audiencia, no invento del modelo
        rec.reason = LanguageManager::trUtf8("La audiencia de este video también mezcla esta pista.",
                                            "Audience of this video also mixes this track.");
        list.push_back(rec);
    }

    return list;
}

void AIService::resolveLocalMatches(std::vector<AIRecommendation>& recs,
                                    const std::vector<juce::File>& localLibraryFiles)
{
    for (auto& r : recs)
    {
        r.isLocal = false;
        r.localFile = juce::File();

        auto cleanTitle = r.title.toLowerCase().trim();
        auto cleanArtist = r.artist.toLowerCase().trim();

        for (const auto& f : localLibraryFiles)
        {
            auto fname = f.getFileNameWithoutExtension().toLowerCase();
            // Check if both artist and title are inside the filename
            if (fname.contains(cleanTitle) || 
               (cleanArtist.isNotEmpty() && fname.contains(cleanArtist) && fname.contains(cleanTitle.substring(0, 5))))
            {
                r.isLocal = true;
                r.localFile = f;
                break;
            }
        }
    }
}
