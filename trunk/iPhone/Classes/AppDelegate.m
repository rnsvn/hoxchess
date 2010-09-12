/***************************************************************************
 *  Copyright 2009-2010 Nevo Hua  <nevo.hua@playxiangqi.com>               *
 *                                                                         * 
 *  This file is part of NevoChess.                                        *
 *                                                                         *
 *  NevoChess is free software: you can redistribute it and/or modify      *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  NevoChess is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with NevoChess.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#import "AppDelegate.h"
#import "Enums.h"
#import "SoundManager.h"

@implementation AppDelegate

@synthesize window;
@synthesize navigationController;

- (void)applicationDidFinishLaunching:(UIApplication *)application
{
    int settingsVersion = [[NSUserDefaults standardUserDefaults] integerForKey:@"settings_version"];
    if (settingsVersion == 0) // Not set?
    {
        // This is the first time this App runs.
        // So, we need to set the default values.
        [[NSUserDefaults standardUserDefaults] setInteger:HC_SETTINGS_VERSION forKey:@"settings_version"];
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"sound_on"];
        [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:@"piece_type"];
        [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:@"board_type"];
        [[NSUserDefaults standardUserDefaults] setInteger:HC_AI_DIFFICULTY_DEFAULT forKey:@"ai_level"];
        [[NSUserDefaults standardUserDefaults] setObject:@"XQWLight" forKey:@"ai_type"];
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"network_autoConnect"];
    }

    [SoundManager sharedInstance].enabled = 
        [[NSUserDefaults standardUserDefaults] boolForKey:@"sound_on"];

    [window addSubview:[navigationController view]];
    navigationController.navigationBarHidden = YES;
    [window makeKeyAndVisible];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Empty.
}

- (void)dealloc
{
    [window release];
    [navigationController release];
    [super dealloc];
}


@end
