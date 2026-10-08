#!/bin/bash
echo "Setting up local instance of HydroServer"

# this should be the root folder where your hydroserver is cloned
export PROJECT_DIRECTORY="${PWD}/hydroserver"
echo $PROJECT_DIRECTORY

if [ ! -d "$PROJECT_DIRECTORY" ]; then 
    echo "project not found"
    git clone git@github.com:hydroserver2/hydroserver.git
    cd "$PROJECT_DIRECTORY"
else
    echo "project found"
    cd "$PROJECT_DIRECTORY"
    # restore local change back to main and pull main from remote
    git restore . 
    git pull origin main -r 
fi 

# Create environment variables for static and media file paths
export HS_STATICFILES="${PWD}/static"
export HS_MEDIAFILES="${PWD}/media"

echo "Spinning local docker container"
cd "deploy/dev"
docker compose up -d 

cd ../..
echo "Setting up Django Backend"
cd "django"
python3 -m pip install -r requirements.txt
echo "Checking for new version of hydroserverpy package"
python3 -m pip install -e ../packages/hydroserverpy

echo "Checking for new version of hydroserver typescript client"
cd .. && cd packages && npm install @hydroserver/client

cd ../django/hydroserver && sed -i '' 's/socket.gethostbyname(hostname)/socket.gethostbyname(hostname + ".local")/g' settings.py && cd ..
PUBLIC_IP=$(curl -s https://ifconfig.me)

cd hydroserver && sed -i '' "s/\[\"127\.0\.0\.1\", \"localhost\", proxy_hostname\]/[\"127.0.0.1\", \"$PUBLIC_IP\", \"localhost\", proxy_hostname]/g" settings.py && cd ..

python3 manage.py migrate
python3 manage.py collectstatic --noinput
export DEFAULT_SUPERUSER_EMAIL=admin@admin.com
export DJANGO_SUPERUSER_PASSWORD=admin
python3 manage.py createsuperuser --noinput
python3 manage.py load_iam_test_data
python3 manage.py load_sta_test_data

echo "Setting up Frontend"
cd .. && cd "apps/data-management"
npm install


cd ../../

echo "Starting HydroServer"

concurrently \
    "cd django && python3 manage.py runserver 0.0.0.0:8000" \
    "cd apps/data-management && npm run dev"
