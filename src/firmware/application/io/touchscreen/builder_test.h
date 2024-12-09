/*

Copyright Igor Petrovic

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#pragma once

#include "touchscreen.h"
#include "hwa_test.h"
#include "models/builder.h"
#include "application/database/builder_test.h"

namespace io::touchscreen
{
    class BuilderTest
    {
        public:
        BuilderTest()
            : _database(_databaseAdmin)
            , _instance(_hwa, _database)
        {}

        // alternative database
        BuilderTest(database::Admin& database)
            : _database(database)
            , _instance(_hwa, _database)
        {}

        HwaTest               _hwa;
        database::BuilderTest _builderDatabase;
        database::Admin&      _databaseAdmin = _builderDatabase.instance();
        Database              _database;
        ModelsBuilder         _models = ModelsBuilder(_hwa);
        Touchscreen           _instance;
    };
}    // namespace io::touchscreen