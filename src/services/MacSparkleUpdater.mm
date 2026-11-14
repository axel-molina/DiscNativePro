#import "MacSparkleUpdater.h"

#if JUCE_MAC

#import <Sparkle/Sparkle.h>

namespace
{
NSString* const kAppcastUrl = @"https://raw.githubusercontent.com/axel-molina/DiscNativePro/main/appcast.xml";

SPUStandardUpdaterController* sharedController = nil;
}

@interface DNPUpdaterDelegate : NSObject <SPUUpdaterDelegate>
@end

@implementation DNPUpdaterDelegate

// Feed por delegado: la app no usa SUFeedURL del Info.plist porque el bundle
// lo genera JUCE/CMake y añadir claves custom ahí es frágil.
- (NSURL*)feedURLForUpdater:(SPUUpdater*)updater
{
    (void) updater;
    return [NSURL URLWithString:kAppcastUrl];
}

// Los updates automáticos (programados) también muestran la alerta estándar
// con "Install on Quit" — no se instala nada sin intervención del driver.
- (BOOL)updater:(SPUUpdater*)updater mayPerformAutomaticUpdateCheck:(NSError* __autoreleasing*)error
{
    (void) updater;
    (void) error;
    return YES;
}

@end

void SparkleBridge::startup()
{
    if (sharedController != nil)
        return;

    static DNPUpdaterDelegate* delegate = [[DNPUpdaterDelegate alloc] init];

    sharedController = [[SPUStandardUpdaterController alloc]
                            initWithUpdaterDelegate:delegate
                                userDriverDelegate:nil];

    NSError* err = nil;
    if (![sharedController.updater startUpdater:&err])
    {
        NSLog(@"Sparkle startUpdater failed: %@",
              (err.localizedDescription != nil ? err.localizedDescription : @"(sin detalle)"));
        sharedController = nil;
    }
}

void SparkleBridge::checkForUpdates()
{
    if (sharedController != nil)
        [sharedController checkForUpdates:nil];
}

void SparkleBridge::shutdown()
{
    sharedController = nil;
}

#endif // JUCE_MAC
