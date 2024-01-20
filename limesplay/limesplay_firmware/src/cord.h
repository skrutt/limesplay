
#include <Vector.h>

class cord 
{
    int16_t val, top, bottom;
    bool dir;
    uint8_t speed;

    public:
        cord(uint16_t bottom = 0, uint16_t top = 0, uint16_t initval = 0) : val(initval * 10), top(top * 10), bottom(bottom * 10)
        {
            speed = random(1, 10);
        }

        void update()
        {
            if (!dir)
            {
                val += speed;
                if (val >= top)
                {
                        dir = true;
                }
            }
            else
            {
                val -= speed;            
                if (val <= bottom)
                {
                        dir = false;
                }
            }
            

        }
        uint16_t get()
        {
            if (val >= 0)
            {
                /* code */
                return val / 10;
            }
            else
                return 0;
        }

};

class point 
{
    cord x,y;

    public:
        point()
        {

        }        
        point(uint16_t xbottom, uint16_t xtop, uint16_t ybottom, uint16_t ytop): x(xbottom, xtop), y(ybottom, ytop)
        {

        }
        point(uint16_t xbottom, uint16_t xtop, uint16_t xinit, uint16_t ybottom, uint16_t ytop, uint16_t yinit): x(xbottom, xtop, xinit), y(ybottom, ytop, yinit)
        {

        }

        void update()
        {
            x.update();
            y.update();

        }
        uint16_t getx()
        {
            return x.get();
        }
        uint16_t gety()
        {
            return y.get();
        }

};

class star 
{
    //Vector <point*> points;
    point * points[20];
    uint8_t number;

    uint8_t ax[4] = {5,5,10,10};
    uint8_t ay[4] = {10,5,10,5}; 

    public:
        star()
        {

        }   
        star(uint8_t no_points)
        {
            number = no_points;
            for(int i = 0; i < no_points; i++)
            {
                //points.push_back(new point(0, 19, random(2, 18), 0, 15, random(2, 14)));
                //points.push_back(new point(0, 19, ax[i], 0, 15, ay[i]));
                points[i] = new point(0, 19, ax[i], 0, 15, ay[i]);
            }
        }

        void update()
        {
            for(uint8_t i = 0; i < number; i++ )    
            {
                points[i]->update();
            }
        }
        void draw(Petter_gfx202 * gfx)
        {
            for(uint8_t i = 0; i < number - 1; i++ )    
            {
                gfx->drawLine(points[i]->getx(), points[i]->gety(), points[i + 1]->getx(), points[i + 1]->gety(), 1);
                //gfx->drawLine(ax[i], ay[i], ax[i + 1], ay[i + 1], 1);
            }
            gfx->drawLine(points[0]->getx(), points[0]->gety(), points[number - 1]->getx(), points[number - 1]->gety(), 1);
            //gfx->drawLine(ax[0], ay[0], ax[number - 1], ay[number - 1], 1);
        }
};